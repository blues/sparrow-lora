// Copyright 2021 Blues Inc.  All rights reserved.
// Use of this source code is governed by licenses granted by the
// copyright holder including that found in the LICENSE file.

#include "app.h"
#include "main.h"

// Forwards
bool commonCmd(char *cmd);
bool commonCharCmd(char ch);

// Log a string "raw", returning the number of bytes sent.  Note that
// this method's signature is compatible with the Notecard's debug tracing
// function, and it must be present even if not debugging
size_t trace(const char *message)
{
#if DEBUGGER_ON
    if (message == NULL || message[0] == '\0') {
        return 0;
    }
    size_t length = strlen(message);
    traceN(message, length);
    return length;
#else
    return 0;
#endif
}

#if DEBUGGER_ON

// The current identity of the subject of the tracing
char traceID[40] = {0};

// Log a string counted
void traceN(const char *message, uint32_t length)
{
    if (message != NULL && message[0] != '\0') {
        MX_DBG(message, length);
    }
}

// Log a newline
void traceNL()
{

    // Output a newline, relying upon lower layers to translate to \r\n
    trace("\n");

    // As a convenience, parse trace input every time we display a full line
    traceInput();

}

// Log a uint32 "raw"
void trace32(uint32_t value)
{
    char buf[24];
    JItoA(value, buf);
    trace(buf);
}

// Log a string with a newline, with trace ID
void traceLn(const char *message)
{
    if (traceID[0] != '\0') {
        trace(traceID);
        trace(" ");
    }
    trace(message);
    trace("\n");
}

// Log two strings with a newline, with trace ID
void trace2Ln(const char *m1, const char *m2)
{
    if (traceID[0] != '\0') {
        trace(traceID);
        trace(" ");
    }
    trace(m1);
    trace(m2);
    trace("\n");
}

// Log three strings with a newline, with trace ID
void trace3Ln(const char *m1, const char *m2, const char *m3)
{
    if (traceID[0] != '\0') {
        trace(traceID);
        trace(" ");
    }
    trace(m1);
    trace(m2);
    trace(m3);
    trace("\n");
}

// Trace a single value
void traceValueLn(const char *m1, uint32_t n1, const char *m2)
{
    if (traceID[0] != '\0') {
        trace(traceID);
        trace(" ");
    }
    trace(m1);
    trace32(n1);
    trace(m2);
    traceNL();
}

// Trace two values
void traceValue2Ln(const char *m1, uint32_t n1, const char *m2, uint32_t n2, const char *m3)
{
    if (traceID[0] != '\0') {
        trace(traceID);
        trace(" ");
    }
    trace(m1);
    trace32(n1);
    trace(m2);
    trace32(n2);
    trace(m3);
    traceNL();
}

// Trace three values
void traceValue3Ln(const char *m1, uint32_t n1, const char *m2, uint32_t n2, const char *m3, uint32_t n3, const char *m4)
{
    if (traceID[0] != '\0') {
        trace(traceID);
        trace(" ");
    }
    trace(m1);
    trace32(n1);
    trace(m2);
    trace32(n2);
    trace(m3);
    trace32(n3);
    trace(m4);
    traceNL();
}

// Trace four values
void traceValue4Ln(const char *m1, uint32_t n1, const char *m2, uint32_t n2, const char *m3, uint32_t n3, const char *m4, uint32_t n4, const char *m5)
{
    if (traceID[0] != '\0') {
        trace(traceID);
        trace(" ");
    }
    trace(m1);
    trace32(n1);
    trace(m2);
    trace32(n2);
    trace(m3);
    trace32(n3);
    trace(m4);
    trace32(n4);
    trace(m5);
    traceNL();
}

// Trace five values
void traceValue5Ln(const char *m1, uint32_t n1, const char *m2, uint32_t n2, const char *m3, uint32_t n3, const char *m4, uint32_t n4, const char *m5, uint32_t n5, const char *m6)
{
    if (traceID[0] != '\0') {
        trace(traceID);
        trace(" ");
    }
    trace(m1);
    trace32(n1);
    trace(m2);
    trace32(n2);
    trace(m3);
    trace32(n3);
    trace(m4);
    trace32(n4);
    trace(m5);
    trace32(n5);
    trace(m6);
    traceNL();
}

// Log a buffer
void traceBufferLn(const char *m1, const char *buffer, uint32_t length)
{
    if (traceID[0] != '\0') {
        trace(traceID);
        trace(" ");
    }
    trace(m1);
    trace(" (");
    trace32(length);
    trace("): ");
    traceNL();
    traceN(buffer, length);
    traceNL();
}

// Clear the identity of what's being processed
void traceClearID(void)
{
    traceID[0] = '\0';
}

// Set the identity of what's being processed
void traceSetID(const char *state, const uint8_t *address, uint32_t requestID)
{
    int addrLen = 6;
    traceID[0] = '\0';
    if (!appIsGateway) {
        char hex[40];
        utilAddressToText(ourAddress, hex, sizeof(hex));
        int len = strlen(hex) > addrLen ? addrLen : strlen(hex);
        strlcat(traceID, &hex[strlen(hex)-len], sizeof(traceID));
    }
    if (state[0] != '\0') {
        if (traceID[0] != '\0') {
            strlcat(traceID, " ", sizeof(traceID));
        }
        strlcat(traceID, state, sizeof(traceID));
    }
    if (address != NULL) {
        if (traceID[0] != '\0') {
            strlcat(traceID, " ", sizeof(traceID));
        }
        if (memcmp(address, gatewayAddress, sizeof(gatewayAddress)) == 0) {
            strlcat(traceID, "gateway", sizeof(traceID));
        } else if (memcmp(address, invalidAddress, sizeof(invalidAddress)) == 0) {
            traceID[0] = '\0';
            requestID = 0;
        } else {
            char hex[40];
            utilAddressToText(address, hex, sizeof(hex));
            int len = strlen(hex) > addrLen ? addrLen : strlen(hex);
            strlcat(traceID, &hex[strlen(hex)-len], sizeof(traceID));
        }
    }
    if (requestID != 0) {
        strlcat(traceID, ":", sizeof(traceID));
        JItoA(requestID, &traceID[strlen(traceID)]);
    }
}

// See if trace input is available
bool traceInputAvailable(void)
{
    return MX_DBG_Available();
}

// Process trace input.
void traceInput(void)
{
    static char cmd[80];
    static uint32_t cmdChars = 0;

    // For now, just echo the input
    while (MX_DBG_Available()) {
        char ch = MX_DBG_Receive(NULL, NULL);
        if (ch == '\r' || ch == '\n') {
            if (cmdChars != 0) {
                cmd[cmdChars] = '\0';
                if (!commonCmd(cmd)) {
                    if (appIsGateway) {
                        gatewayCmd(cmd);
                    } else {
                        sensorCmd(cmd);
                    }
                }
                cmdChars = 0;
            }
            continue;
        }
        if (ch < ' ' || ch >= 0x7f) {
            continue;
        }
        if (commonCharCmd(ch)) {
            continue;
        }
        if (cmdChars >= sizeof(cmd)-1) {
            continue;
        }
        cmd[cmdChars++] = ch;
    }

}

// Execute console command that is a single character
bool commonCharCmd(char ch)
{
    if (ch == '=') {
        uint32_t localTimeSecs = NoteTimeST();
        int64_t localTimeMs = TIMER_IF_GetTimeMs();
        if (appIsGateway) {
            J *rsp = NoteRequestResponse(NoteNewRequest("card.time"));
            if (rsp != NULL) {
                JTIME cardTimeSecs = JGetInt(rsp, "time");
                int diffLocalCard = (int) ((int64_t) localTimeSecs - (int64_t) cardTimeSecs);
                int diffLocalBoot = (int) ((int64_t) localTimeSecs - (int64_t) gatewayBootTime);
                traceValue5Ln("ms:", (uint32_t) localTimeMs, " time:", localTimeSecs, " card:", cardTimeSecs, " diff:", diffLocalCard, " bootSecs:", diffLocalBoot, "");
                NoteDeleteResponse(rsp);
            }
        } else {
            traceValue2Ln("ms:", localTimeMs, " time:", (uint32_t) localTimeSecs, "");
        }
        return true;
    }
    return false;
}

// Execute console command
bool commonCmd(char *cmd)
{
    if (strcmp(cmd, "trace") == 0 || strcmp(cmd, "t") == 0) {
        trace("TRACE ON");
        traceNL();
        NoteSetFnDebugOutput(trace);
        MX_DBG_Enable();
        return true;
    }
    if (strcmp(cmd, "restart") == 0) {
        trace("restarting...");
        traceNL();
        HAL_Delay(1000);
        NVIC_SystemReset();
    }
    return false;
}

#endif // DEBUGGER_ON
