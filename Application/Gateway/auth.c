// Copyright 2021 Blues Inc.  All rights reserved.
// Use of this source code is governed by licenses granted by the
// copyright holder including that found in the LICENSE file.

#include "app.h"

// Allowed requests
char *allowedRequests[] = {
    "env.get",
    "note.add",
    "note.template",
    "hub.log",
    ""
};

// Check whether or not the sensor may issue this request to the notecard.  If
// authorized, return NULL.  Otherwise, return the rsp that should be given
// back to the sensor instead of executing the request.  Note that in all
// cases, req should not be freed.  Note that this method may perform any
// kind of tests or transformations on the request that it likes (including
// checking for certain notefiles), and is free to modify the incoming request
// in order to bring it into compliance.
J *authRequest(uint8_t *sensorAddress, char *sensorName, char *sensorLocationOLC, J *req)
{
    J *rsp = NULL;

    // Look up the request in a list of allowed request types.
    bool allowed = false;
    char *reqType = JGetString(req, "req");
    for (int i=0;; i++) {
        if (allowedRequests[i][0] == '\0') {
            break;
        }
        char *allowedreq = allowedRequests[i];
        if (strcmp(reqType, allowedreq) == 0) {
            allowed = true;
            break;
        }
    }
    if (!allowed) {
        rsp = JCreateObject();
        JAddStringToObject(rsp, "err", "request type not allowed");
        return rsp;
    }

    // In order to save bandwidth over the air, look for a
    // "file" parameter that begins with the "*" character,
    // and if so subtitute the sensor's ID.  This is just an
    // accomodation that enables a sensor to reduce the
    // bandwidth of the request going over the air.
    const char *file = JGetString(req, "file");
    if (file[0] == '*') {
        char notefileID[64];
        utilAddressToText(sensorAddress, notefileID, sizeof(notefileID));
        strlcat(notefileID, &file[1], sizeof(notefileID));
        JDeleteItemFromObject(req, "file");
        JAddStringToObject(req, "file", notefileID);
    }

    // If this is a note.add, set the location as configured in env vars
    if (strcmp(reqType, "note.add") == 0 && sensorLocationOLC[0] != '\0') {
        JDeleteItemFromObject(req, "olc");
        JAddStringToObject(req, "olc", sensorLocationOLC);
    }

    // Done
    return rsp;

}
