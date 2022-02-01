// Copyright 2022 Blues Inc.  All rights reserved.
// Use of this source code is governed by licenses granted by the
// copyright holder including that found in the LICENSE file.

#include "main.h"
#include "app.h"

// Check to see if a firmware update is available, and perform the update if it is.  If
// the update isn't available, return false.  If the update was available but the update
// failed for whatever reason, return true.  (When true is returned, the hub mode must
// be restored by the caller.)  If the update was available and succeeded, then don't
// return from this call - just reboot.
bool noteFirmwareUpdateIfAvailable()
{

    // Get flash programming parameters
    uint8_t *flashCodeActiveBase, *flashCodeDFUBase;
    uint32_t flashCodeMaxBytes, flashCodeMaxPages;
    flashCodeParams(&flashCodeActiveBase, &flashCodeDFUBase, &flashCodeMaxBytes, &flashCodeMaxPages);

    // Check status, and determine both if there is an image ready, and if the image is NEW.
    bool imageIsReady = false;
    bool imageIsSameAsCurrent = false;
    char imageMD5[NOTE_MD5_HASH_STRING_SIZE] = {0};
    char currentMD5[NOTE_MD5_HASH_STRING_SIZE] = {0};
    uint32_t imageLength = 0;
    J *rsp = NoteRequestResponse(NoteNewRequest("dfu.status"));
    if (rsp != NULL) {
        if (strcmp(JGetString(rsp, "mode"), "ready") == 0) {
            imageIsReady = true;
            J *body = JGetObjectItem(rsp, "body");
            if (body != NULL) {
                imageLength = JGetInt(body, "length");
                if (imageLength > flashCodeMaxBytes) {
                    APP_PRINTF("dfu: can't use image because it's too large\r\n");
                    return false;
                }
                strlcpy(imageMD5, JGetString(body, "md5"), sizeof(imageMD5));
                uint32_t currentImageSize = MX_Image_Size();
                NoteMD5HashString(flashCodeActiveBase, currentImageSize, currentMD5, sizeof(currentMD5));
                imageIsSameAsCurrent = (strcmp(currentMD5, imageMD5) == 0);
                if (imageIsSameAsCurrent) {
                    // Tell the notecard that the DFU is completed
#ifndef DFU_TESTING
                    J *req = NoteNewRequest("dfu.status");
                    if (req != NULL) {
                        JAddBoolToObject(req, "stop", true);
                        NoteRequest(req);
                    }
#endif
                    return false;
                }
                APP_PRINTF("dfu: replacing current image because of MD5 mismatch\r\n");
            }
        }
        NoteDeleteResponse(rsp);
    }

    // Exit if same version or no DFU to process
    if (!imageIsReady || imageIsSameAsCurrent || imageLength == 0) {
        return false;
    }

    // Compute the number of pages that the image will take
    uint32_t flashCodePages = (imageLength / FLASH_PAGE_SIZE);
    if ((imageLength % FLASH_PAGE_SIZE) != 0) {
        flashCodePages++;
    }

    // Enter DFU mode.  Note that the Notecard will automatically switch us back out of
    // DFU mode after 15m, so we don't leave the notecard in a bad state if we had a problem here.
    J *req = NoteNewRequest("hub.set");
    if (req == NULL) {
        return false;
    }
    JAddStringToObject(req, "mode", "dfu");
    NoteRequest(req);

    // Wait until we have successfully entered the mode.  The fact that this loop isn't
    // just an infinitely loop is simply defensive programming.  If for some odd reason
    // we don't enter DFU mode, we'll eventually come back here on the next DFU poll.
    bool inDFUMode = false;
    JTIME beganDFUModeCheckTime = NoteTimeST();
    while (!inDFUMode && NoteTimeST() < beganDFUModeCheckTime + (2*60)) {
        J *rsp = NoteRequestResponse(NoteNewRequest("dfu.get"));
        if (rsp != NULL) {
            if (!NoteResponseError(rsp)) {
                inDFUMode = true;
            }
            NoteDeleteResponse(rsp);
        }
        if (!inDFUMode) {
            APP_PRINTF("dfu: waiting for access to DFU image\r\n");
            HAL_Delay(2500);
        }
    }

    // If we failed, leave DFU mode immediately
    if (!inDFUMode) {
        APP_PRINTF("dfu: timeout waiting for notecard to enter DFU mode\r\n");
        return true;
    }

    // The image is ready.
    APP_PRINTF("dfu: beginning firmware update\r\n");

    // Loop over received chunks.  The chunk size is arbitrary, so we'll use page size
    // just so that the flash programming code doesn't need to do any buffering.
    int offset = 0;
    int chunklen = FLASH_PAGE_SIZE;
    int left = imageLength;
    NoteMD5Context md5Context;
    NoteMD5Init(&md5Context);
    while (left) {

        // Read next chunk from card
        int thislen = chunklen;
        if (left < thislen) {
            thislen = left;
        }

        // If anywhere, this is the location of the highest probability of I/O error
        // on the I2C or serial bus, simply because of the amount of data being transferred.
        // As such, it's a conservative measure just to retry.
        char *payload = NULL;
        for (int retry=0; retry<5; retry++) {
            APP_PRINTF("dfu: reading chunk (offset:%d length:%d try:%d)\r\n", offset, thislen, retry+1);
            // Request the next chunk from the notecard
            J *req = NoteNewRequest("dfu.get");
            if (req == NULL) {
                APP_PRINTF("dfu: insufficient memory\r\n");
                return true;
            }
            JAddNumberToObject(req, "offset", offset);
            JAddNumberToObject(req, "length", thislen);
            J *rsp = NoteRequestResponse(req);
            if (rsp == NULL) {
                APP_PRINTF("dfu: insufficient memory\r\n");
                return true;
            }
            if (NoteResponseError(rsp)) {

                APP_PRINTF("dfu: error on read: %s\r\n", JGetString(rsp, "err"));

            } else {

                char *payloadB64 = JGetString(rsp, "payload");
                if (payloadB64[0] == '\0') {
                    APP_PRINTF("dfu: no payload\r\n");
                    NoteDeleteResponse(rsp);
                    return true;
                }
                payload = (char *) malloc(JB64DecodeLen(payloadB64));
                if (payload == NULL) {
                    APP_PRINTF("dfu: can't allocate payload decode buffer\r\n");
                    NoteDeleteResponse(rsp);
                    return true;
                }
                int actuallen = JB64Decode(payload, payloadB64);
                const char *expectedMD5 = JGetString(rsp, "status");;
                char chunkMD5[NOTE_MD5_HASH_STRING_SIZE] = {0};
                NoteMD5HashString((uint8_t *)payload, actuallen, chunkMD5, sizeof(chunkMD5));
                if (actuallen == thislen && strcmp(chunkMD5, expectedMD5) == 0) {
                    NoteDeleteResponse(rsp);
                    break;
                }

                free(payload);
                payload = NULL;

                if (thislen != actuallen) {
                    APP_PRINTF("dfu: decoded data not the correct length (%d != actual %d)", thislen, actuallen);
                } else {
                    APP_PRINTF("dfu: %d-byte decoded data MD5 mismatch\r\n", actuallen);
                }

            }

            NoteDeleteResponse(rsp);

        }
        if (payload == NULL) {
            APP_PRINTF("dfu: unrecoverable error on read\r\n");
            return true;
        }

        // MD5 the chunk
        NoteMD5Update(&md5Context, (uint8_t *)payload, thislen);

        // Write the chunk
        bool success = flashWrite(&flashCodeDFUBase[offset], payload, thislen);
        if (!success) {
            free(payload);
            return true;
        }

        // Move to next chunk
        free(payload);
        APP_PRINTF("dfu: successfully transferred offset:%d len:%d\r\n", offset, thislen);
        offset += thislen;
        left -= thislen;

    }

    // Exit DFU mode.  (Had we not done this, the Notecard exits DFU mode automatically after 15m.)
#ifndef DFU_TESTING
    req = NoteNewRequest("hub.set");
    if (req != NULL) {
        JAddStringToObject(req, "mode", "dfu-completed");
        NoteRequest(req);
    }
#endif

    // Completed, so we now validate the MD5
    uint8_t md5Hash[NOTE_MD5_HASH_SIZE];
    NoteMD5Final(md5Hash, &md5Context);
    char md5HashString[NOTE_MD5_HASH_STRING_SIZE];
    NoteMD5HashToString(md5Hash, md5HashString, sizeof(md5HashString));
    APP_PRINTF("dfu:    MD5 of image: %s\r\n", imageMD5);
    APP_PRINTF("dfu: MD5 of download: %s\r\n", md5HashString);
    if (strcmp(imageMD5, md5HashString) != 0) {
        APP_PRINTF("MD5 MISMATCH - ABANDONING DFU\r\n");
        return true;
    }

    // Clear out the DFU image
#ifndef DFU_TESTING
    req = NoteNewRequest("dfu.status");
    if (req != NULL) {
        JAddBoolToObject(req, "stop", true);
        NoteRequest(req);
    }
#endif

    // Jump to the DFU copying method
    APP_PRINTF("dfu: copy %d pages to active partition", flashCodePages);
    dfuLoader(flashCodeActiveBase, flashCodeDFUBase, flashCodePages);

    // (will not return here)
    return true;

}
