// Copyright 2022 Blues Inc.  All rights reserved.
// Use of this source code is governed by licenses granted by the
// copyright holder including that found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "framework.h"
#include "main.h"
#include "stm32wlxx_ll_gpio.h"

// The current identity of the subject of the tracing
char traceID[40] = {0};

// Forwards
bool commonCmd(char *cmd);
bool commonCharCmd(char ch);
void probePin(GPIO_TypeDef *GPIOx, char *pinprefix);

// Return the trace prefix
char *tracePeer()
{
    return traceID;
}

// Clear the identity of what's being processed
void traceClearID(void)
{
    traceID[0] = '\0';
}

// Set the identity of what's being processed
void traceSetID(const char *state, const uint8_t *address, uint32_t requestID)
{
    size_t addrLen = 6;
    traceID[0] = '\0';
    if (!appIsGateway) {
        char hex[40];
        utilAddressToText(ourAddress, hex, sizeof(hex));
        size_t len = strlen(hex) > addrLen ? addrLen : strlen(hex);
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
            size_t len = strlen(hex) > addrLen ? addrLen : strlen(hex);
            strlcat(traceID, &hex[strlen(hex)-len], sizeof(traceID));
        }
    }
    if (requestID != 0) {
        strlcat(traceID, ":", sizeof(traceID));
        JItoA(requestID, &traceID[strlen(traceID)]);
    }
}

#if DEBUGGER_ON

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

    // Start by assuming \n is term char, but accept any of \r or \r\n or \n
    static char cmdTerm = '\n';
    static char cmdSkip = 0;

    // For now, just echo the input
    while (MX_DBG_Available()) {
        char ch = MX_DBG_Receive(NULL, NULL);
        if (ch == '\r') {
            cmdTerm = '\r';
            cmdSkip = '\n';
        }
        if (ch == cmdSkip) {
            continue;
        }
        if (ch == cmdTerm) {
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

    // Display time
    if (ch == '=') {
        MX_DBG_Enable();
        uint32_t localTimeSecs = NoteTimeST();
        int64_t localTimeMs = TIMER_IF_GetTimeMs();
        if (appIsGateway) {
            NoteSuspendTransactionDebug();
            J *rsp = NoteRequestResponse(NoteNewRequest("card.time"));
            NoteResumeTransactionDebug();
            if (rsp != NULL) {
                JTIME cardTimeSecs = JGetInt(rsp, "time");
                int diffLocalCard = (int) ((int64_t) localTimeSecs - (int64_t) cardTimeSecs);
                int diffLocalBoot = (int) ((int64_t) localTimeSecs - (int64_t) gatewayBootTime);
                APP_PRINTF("ms:%d time:%d card:%d diff:%d bootSecs:%d\r\n",
                           (uint32_t) localTimeMs, localTimeSecs, cardTimeSecs, diffLocalCard, diffLocalBoot);
                NoteDeleteResponse(rsp);
            }
        } else {
            APP_PRINTF("ms:%d time:%d\r\n", localTimeMs, (uint32_t) localTimeSecs);
        }
        return true;
    }

    // Display voltage
#if (SPARROW_DEVICE != BOARD_NUCLEO)
    if (ch == '+') {
        MX_DBG_Enable();
        APP_PRINTF("bat: %d millivolts\r\r", (uint32_t) (MX_ADC_A0_Voltage() * 1000.0));
        return true;
    }
#endif

    return false;
}

// Probe GPIO port to see what state it's in
void probePin(GPIO_TypeDef *GPIOx, char *pinprefix)
{
#if (LL_GPIO_PIN_0 != 1)
#error huh?
#endif
    for (int pini=0; pini<16; pini++) {
        int pin = 1 << pini;
        uint32_t mode = LL_GPIO_GetPinMode(GPIOx, pin);
        const char *modestr = "unknown";
        bool analog = false;
        switch (mode) {
        case LL_GPIO_MODE_INPUT:
            switch (LL_GPIO_GetPinPull(GPIOx, pin)) {
            case LL_GPIO_PULL_NO:
                modestr = "input-nopull";
                break;
            case LL_GPIO_PULL_UP:
                modestr = "input-pullup";
                break;
            case LL_GPIO_PULL_DOWN:
                modestr = "input-pulldown";
                break;
            }
            break;
        case LL_GPIO_MODE_OUTPUT:
            switch (LL_GPIO_GetPinOutputType(GPIOx, pin)) {
            case LL_GPIO_OUTPUT_PUSHPULL:
                modestr = "output-pp";
                break;
            case LL_GPIO_OUTPUT_OPENDRAIN:
                modestr = "output-od";
                break;
            }
            break;
        case LL_GPIO_MODE_ALTERNATE:
            modestr = "alternate";
            break;
        case LL_GPIO_MODE_ANALOG:
            modestr = "analog";
            analog = true;
            break;
        }
        const char *set = "";
        if (!analog && GPIO_PIN_RESET != HAL_GPIO_ReadPin(GPIOx, pin)) {
            set = " HIGH";
        }
        APP_PRINTF("%s%d: %s%s\r\n", pinprefix, pini, modestr, set);
    }
}

// Trace output from the notecard
size_t trace(const char *message)
{
    APP_PRINTF("%s", message);
    return strlen(message);
}

// Execute console command
bool commonCmd(char *cmd)
{

    // Turn trace on/off
    if (strcmp(cmd, "trace") == 0 || strcmp(cmd, "t") == 0) {
        MX_DBG_Enable();
        APP_PRINTF("TRACE ON\r\n");
        return true;
    }

    // Turn notecard I/O trace on/off
    if (appIsGateway && (strcmp(cmd, "note") == 0 || strcmp(cmd, "n") == 0)) {
        NoteSetFnDebugOutput(trace);
        MX_DBG_Enable();
        APP_PRINTF("NOTECARD TRACE ON\r\n");
        return true;
    }

    // Perform a self-test
    if (strcmp(cmd, "{\"req\":\"card.test\"}") == 0 || strcmp(cmd, "test") == 0) {
        post(POST_GPIO);
    }
    if (strcmp(cmd, "{\"req\":\"card.test\",\"sku\":\"ref\"}") == 0 || strcmp(cmd, "test-ref") == 0) {
        post(POST_GPIO|POST_BME);
    }

    // Restart the module
    if (strcmp(cmd, "restart") == 0) {
        MX_DBG_Enable();
        APP_PRINTF("restarting...\r\n");
        HAL_Delay(1000);
        NVIC_SystemReset();
    }

    // When debugging power issues, show state of all pins
    if (strcmp(cmd, "probe") == 0) {
        MX_DBG_Enable();
        probePin(GPIOA, "PA");
        probePin(GPIOB, "PB");
        probePin(GPIOC, "PC");
        probePin(GPIOH, "PH");
        char buf[128];
        MY_ActivePeripherals(buf, sizeof(buf));
        APP_PRINTF("%s\r\n", buf);
        return true;
    }

    return false;
}

#endif // DEBUGGER_ON
