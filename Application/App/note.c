// Copyright 2021 Blues Inc.  All rights reserved.
// Use of this source code is governed by licenses granted by the
// copyright holder including that found in the LICENSE file.

#include "main.h"
#include "app.h"

// For Notecard I2C I/O using ST HAL
extern I2C_HandleTypeDef hi2c2;

// Forwards
bool noteI2CReset(uint16_t DevAddress);
const char *noteI2CTransmit(uint16_t DevAddress, uint8_t* pBuffer, uint16_t Size);
const char *noteI2CReceive(uint16_t DevAddress, uint8_t* pBuffer, uint16_t Size, uint32_t *available);
void noteDelay(uint32_t ms);
long unsigned int noteMillis(void);
void noteBeginTransaction(void);
void noteEndTransaction(void);

// Initialize the note subsystem
bool noteInit()
{

    // Register callbacks with note-c subsystem that it needs for I/O, memory, timer
    NoteSetFn(malloc, free, noteDelay, noteMillis);

    // On the gateway, register I2C
    NoteSetFnMutex(NULL, NULL, noteBeginTransaction, noteEndTransaction);
    NoteSetFnI2C(NOTE_I2C_ADDR_DEFAULT, NOTE_I2C_MAX_DEFAULT, noteI2CReset, noteI2CTransmit, noteI2CReceive);

    // Power-on I2C to see if the device is there
    GPIO_InitTypeDef init = {0};
    init.Mode = GPIO_MODE_OUTPUT_PP;
    init.Pull = GPIO_NOPULL;
    init.Speed = GPIO_SPEED_FREQ_HIGH;
    init.Pin = I2C_POWER_Pin;
    HAL_GPIO_Init(I2C_POWER_GPIO_Port, &init);
    HAL_GPIO_WritePin(I2C_POWER_GPIO_Port, I2C_POWER_Pin, GPIO_PIN_SET);
    HAL_Delay(250);
    MX_I2C2_Init();

    // Test to see if a notecard is present
    bool notecardFound = false;
    for (int i=0; i<3; i++) {
        char ver[4];
        if (NoteGetVersion(ver, sizeof(ver))) {
            notecardFound = true;
            break;
        }
        HAL_Delay(1000);
    }
    if (!notecardFound) {

        // Remove hooks and disable
        NoteSetFnMutex(NULL, NULL, NULL,NULL);
        NoteSetFnDisabled();

        // Power-off I2C
        MX_I2C2_DeInit();
        HAL_GPIO_WritePin(I2C_POWER_GPIO_Port, I2C_POWER_Pin, GPIO_PIN_RESET);
        
        return false;
    }

    // Set the debug output function
    NoteSetFnDebugOutput(trace);

    // Success
    return true;

}

// Setup the Notecard parameters
bool noteSetup()
{
    bool initialized = false;

    // Set the essential info, retrying in case of I/O error during startup
    // simply because this is so essential
    for (int i=0; i<5; i++) {
        J *req = NoteNewRequest("hub.set");
        if (req != NULL) {
            JAddStringToObject(req, "product", NOTECARD_PRODUCT_UID);
            JAddStringToObject(req, "mode", NOTECARD_CONNECTION_MODE);
            JAddNumberToObject(req, "outbound", NOTECARD_OUTBOUND_PERIOD_MINS);
            JAddNumberToObject(req, "inbound", NOTECARD_INBOUND_PERIOD_MINS);
            JAddBoolToObject(req, "align", true);
            if (NoteRequest(req)) {
                initialized = true;
                break;
            }
            HAL_Delay(1000);
        }
    }

    // In order to completely close the loop with Notehub as to which version of our firmware is
    // currently running, make sure that the Notecard and thus the Notehub has the exact string
    // that is embedded within our image that indicates the firmware version.
    J *req = NoteNewRequest("dfu.status");
    if (req != NULL) {
        JAddStringToObject(req, "version", appFirmwareVersion());
        NoteRequest(req);
    }

    // Done
    return initialized;

}

// Begin a notecard transaction which may involve many I2C transactions
void noteBeginTransaction()
{
    MX_I2C2_Init();
}

// End a notecard transaction
void noteEndTransaction()
{
    MX_I2C2_DeInit();
}

// Arduino-like delay function
void noteDelay(uint32_t ms)
{
    HAL_Delay(ms);
}

// Arduino-like millis() function
long unsigned int noteMillis()
{
    return (long unsigned int) TIMER_IF_GetTimeMs();
}

// I2C reset procedure, called before any I/O and called again upon I/O error
bool noteI2CReset(uint16_t DevAddress)
{
    MX_I2C2_DeInit();
    MX_I2C2_Init();
    return true;
}

// Transmits in master mode an amount of data, in blocking mode.     The address
// is the actual address; the caller should have shifted it right so that the
// low bit is NOT the read/write bit. An error message is returned, else NULL if success.
const char *noteI2CTransmit(uint16_t DevAddress, uint8_t* pBuffer, uint16_t Size)
{
    char *errstr;
    int writelen = sizeof(uint8_t) + Size;
    uint8_t *writebuf = malloc(writelen);
    if (writebuf == NULL) {
        return "i2c: insufficient memory (write)";
    }

    // Retry so that we're resiliant in the context of customer designs that have unclean SDA/SCL signals
    writebuf[0] = Size;
    memcpy(&writebuf[1], pBuffer, Size);
    errstr = "i2c: write error {io}";
    for (int i=0; i<5; i++) {
        HAL_StatusTypeDef err_code = HAL_I2C_Master_Transmit(&hi2c2, DevAddress<<1, writebuf, writelen, 250);
        if (err_code == HAL_OK) {
            errstr = NULL;
            break;
        }
        HAL_Delay(100);
    }
    if (errstr != NULL) {
        free(writebuf);
        return errstr;
    }

    // Done
    free(writebuf);
    return NULL;

}

// Receives in master mode an amount of data in blocking mode. An error mesage returned, else NULL if success.
const char *noteI2CReceive(uint16_t DevAddress, uint8_t* pBuffer, uint16_t Size, uint32_t *available)
{
    const char *errstr;

    // Retry so that we're resiliant in the context of customer designs that have unclean SDA/SCL signals
    uint8_t hdr[2];
    hdr[0] = (uint8_t) 0;
    hdr[1] = (uint8_t) Size;
    errstr = "i2c: write error {io}";
    for (int i=0; i<5; i++) {
        HAL_StatusTypeDef err_code = HAL_I2C_Master_Transmit(&hi2c2, DevAddress<<1, hdr, sizeof(hdr), 250);
        if (err_code == HAL_OK) {
            errstr = NULL;
            break;
        }
        HAL_Delay(100);
    }
    if (errstr != NULL) {
        return errstr;
    }

    // Only receive if we successfully began transmission
    int readlen = Size + (sizeof(uint8_t)*2);
    uint8_t *readbuf = malloc(readlen);
    if (readbuf == NULL) {
        return "i2c: insufficient memory (read)";
    }

    // Retry so that we're resiliant in the context of customer designs that have unclean SDA/SCL signals
    errstr = "i2c: read error {io}";
    for (int i=0; i<5; i++) {
        HAL_StatusTypeDef err_code = HAL_I2C_Master_Receive(&hi2c2, DevAddress<<1, readbuf, readlen, 10);
        if (err_code == HAL_OK) {
            errstr = NULL;
            break;
        }
        HAL_Delay(100);
    }
    if (errstr != NULL) {
        free(readbuf);
        return errstr;
    }

    uint8_t availbyte = readbuf[0];
    uint8_t goodbyte = readbuf[1];
    if (goodbyte != Size) {
        free(readbuf);
        return "i2c: incorrect amount of data";
    }

    *available = availbyte;
    memcpy(pBuffer, &readbuf[2], Size);
    free(readbuf);
    return NULL;

}

// Send a note to the gateway async
void noteSendToGatewayAsync(J *req, bool responseExpected)
{

    // Add the time to the request, because it may be quite a while
    // to acquire a transmit window and we'd like the time to be
    // as accurate as it can be.
    if (NoteTimeValidST()) {
        JAddNumberToObject(req, "time", NoteTimeST());
    }

    // Enqueue it to the gateway
    sensorSendReqToGateway(req, responseExpected);

}
