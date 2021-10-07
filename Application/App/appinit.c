// Copyright 2021 Blues Inc.  All rights reserved.
// Use of this source code is governed by licenses granted by the
// copyright holder including that found in the LICENSE file.

#include "main.h"
#include "stm32_seq.h"
#include "utilities_def.h"
#include "app.h"

// App Role
bool appIsGateway = false;
int64_t appBootMs = 0;
uint32_t gatewayBootTime = 0;
bool buttonHeldAtBoot = false;

// Forwards
void registerApp(void);
void ioInit(void);
void unpack32(uint8_t *p, uint32_t value);
int tristate(uint16_t pin, GPIO_TypeDef *port);

// Get the version of the image
const char *appFirmwareVersion()
{
    return __DATE__ " " __TIME__;
}

// Main app processing loop
void MX_AppMain(void)
{

    // Initialize GPIOs
    ioInit();

    // Determine and display our own device address
    memset(ourAddress, 0, sizeof(ourAddress));
    unpack32(&ourAddress[0], HAL_GetUIDw0());
    unpack32(&ourAddress[4], HAL_GetUIDw1());
    unpack32(&ourAddress[8], HAL_GetUIDw2());
    utilAddressToText(ourAddress, ourAddressText, sizeof(ourAddressText));
    trace("{\"id\":\"");
    trace(ourAddressText);
    traceLn("\"}");
    traceLn("");

    // Remember the time when we were booted
    appBootMs = TIMER_IF_GetTimeMs();

    // Conditionally disable debugging
    if (!buttonHeldAtBoot && !MX_DBG_Active()) {
        traceLn("CONSOLE TRACE DISABLED");
        NoteSetFnDebugOutput(NULL);
        MX_DBG_Disable();
    } else {
        traceLn("CONSOLE TRACE ENABLED");
    }

    // Initialize the Notecard
    appIsGateway = noteInit();

    // Show the firmware version
    traceLn(appFirmwareVersion());

    // On the gateway, prep for flash DFU
    if (appIsGateway) {
        flashDFUInit();
    }

    // Send setup parameters to the Notecard and load config
    if (appIsGateway) {
        noteSetup();
        gatewaySetEnvVarDefaults();
    }

    // Blink LED until the time is available, because the sensors depend
    // upon the fact thst the gateway knows what time it is.
    if (appIsGateway) {
        ledReset();
        while ( !NoteTimeValidST() || !gatewayEnvVarsLoaded() ) {
            if (ledButtonCheck() == BUTTON_HELD) {
                flashConfigFactoryReset();
            }
            ledWalk();
            HAL_Delay(750);
        }
        for (int i=0; i<60 && !NoteRegion(NULL, NULL, NULL, NULL); i++) {
            ledWalk();
            HAL_Delay(500);
        }
        gatewayBootTime = NoteTimeST();
    }
    ledReset();

    // Initialize the radio
    radioInit();

    // Load configuration from flash and start the main task
    flashConfigLoad();
    registerApp();

    // Loop, processing tasks registered with the sequencer
    while (true) {
        UTIL_SEQ_Run(UTIL_SEQ_DEFAULT);
    }

}

// ISR for interrupts to be processed by the app
void MX_AppISR(uint16_t GPIO_Pin)
{

    // If the debug terminal pressed a button, wake up
    if (GPIO_Pin == 0) {
        appTraceWakeup();
        return;
    }

    // Do special processing of button pin, because we
    // do checking for HOLD and other things.  Once
    // we determine that it's not a hold, we will
    // send the interrupt to sensors through a
    // different but compatible path.
    if ((GPIO_Pin & BUTTON1_Pin) != 0) {
        GPIO_Pin &= ~BUTTON1_Pin;
        appButtonWakeup();
    }

    // Dispatch the interrupts to all sensors
    if (!appIsGateway) {
        schedDispatchISR(GPIO_Pin);
    }

}

// Get the value of a tri-state GPIO
#define TRISTATE_FLOAT  0
#define TRISTATE_HIGH   1
#define TRISTATE_LOW    2
int tristate(uint16_t pin, GPIO_TypeDef *port)
{
    GPIO_InitTypeDef  gpio_init_structure = {0};
    gpio_init_structure.Mode = GPIO_MODE_INPUT;
    gpio_init_structure.Pull = GPIO_PULLUP;
    gpio_init_structure.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio_init_structure.Pin = pin;
    HAL_GPIO_Init(port, &gpio_init_structure);
    bool pulledHigh = (GPIO_PIN_SET == HAL_GPIO_ReadPin(port, pin));
    gpio_init_structure.Mode = GPIO_MODE_INPUT;
    gpio_init_structure.Pull = GPIO_PULLDOWN;
    gpio_init_structure.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio_init_structure.Pin = pin;
    HAL_GPIO_Init(port, &gpio_init_structure);
    bool pulledLow = (GPIO_PIN_RESET == HAL_GPIO_ReadPin(port, pin));
    gpio_init_structure.Mode = GPIO_MODE_INPUT;
    gpio_init_structure.Pull = GPIO_NOPULL;
    gpio_init_structure.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio_init_structure.Pin = pin;
    HAL_GPIO_Init(port, &gpio_init_structure);
    bool high = (GPIO_PIN_SET == HAL_GPIO_ReadPin(port, pin));
    gpio_init_structure.Mode = GPIO_MODE_ANALOG;
    gpio_init_structure.Pull = GPIO_NOPULL;
    gpio_init_structure.Speed = GPIO_SPEED_FREQ_LOW;
    gpio_init_structure.Pin = pin;
    HAL_GPIO_Init(port, &gpio_init_structure);
    if (pulledHigh && pulledLow) {
        return TRISTATE_FLOAT;
    }
    return (high ? TRISTATE_HIGH : TRISTATE_LOW);
}

// Initialize app hardware I/O
void ioInit(void)
{
    GPIO_InitTypeDef  gpio_init_structure = {0};

    // Compute the RF frequency based on the region switch settings.  Note that
    // we power these pins with LED_RED so that they aren't a constant current
    // draw on the system.
#if (CURRENT_BOARD!=BOARD_NUCLEO)
    gpio_init_structure.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init_structure.Pull = GPIO_NOPULL;
    gpio_init_structure.Speed = GPIO_SPEED_FREQ_LOW;
    gpio_init_structure.Pin = LED_RED_Pin;
    HAL_GPIO_Init(LED_RED_GPIO_Port, &gpio_init_structure);
    HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_SET);
    int gpio0 = tristate(RFSEL_0_Pin, RFSEL_0_GPIO_Port);
    int gpio1 = tristate(RFSEL_1_Pin, RFSEL_1_GPIO_Port);
    int value;
    if (gpio0 == TRISTATE_FLOAT && gpio1 == TRISTATE_FLOAT) {
        value = 0;
    } else if (gpio0 == TRISTATE_HIGH && gpio1 == TRISTATE_FLOAT) {
        value = 1;
    } else if (gpio0 == TRISTATE_LOW && gpio1 == TRISTATE_FLOAT) {
        value = 2;
    } else if (gpio0 == TRISTATE_HIGH && gpio1 == TRISTATE_FLOAT) {
        value = 1;
    } else if (gpio0 == TRISTATE_FLOAT && gpio1 == TRISTATE_HIGH) {
        value = 3;
    } else if (gpio0 == TRISTATE_HIGH && gpio1 == TRISTATE_HIGH) {
        value = 4;
    } else if (gpio0 == TRISTATE_LOW && gpio1 == TRISTATE_HIGH) {
        value = 5;
    } else if (gpio0 == TRISTATE_HIGH && gpio1 == TRISTATE_HIGH) {
        value = 4;
    } else if (gpio0 == TRISTATE_FLOAT && gpio1 == TRISTATE_LOW) {
        value = 6;
    } else if (gpio0 == TRISTATE_HIGH && gpio1 == TRISTATE_LOW) {
        value = 7;
    } else if (gpio0 == TRISTATE_LOW && gpio1 == TRISTATE_LOW) {
        value = 8;
    } else if (gpio0 == TRISTATE_HIGH && gpio1 == TRISTATE_LOW) {
        value = 7;
    } else if (gpio0 == TRISTATE_FLOAT && gpio1 == TRISTATE_HIGH) {
        value = 3;
    } else if (gpio0 == TRISTATE_HIGH && gpio1 == TRISTATE_HIGH) {
        value = 4;
    } else if (gpio0 == TRISTATE_LOW && gpio1 == TRISTATE_HIGH) {
        value = 5;
    } else if (gpio0 == TRISTATE_HIGH && gpio1 == TRISTATE_HIGH) {
        value = 4;
    }
    uint32_t freq = 915000000;
    switch (value) {
    default:
    case 0:
        freq = 915000000;  // OFF OFF OFF OFF (US915 & AU915)
        break;
    case 1:
        freq = 923000000;  //  ON OFF OFF OFF (AS923)
        break;
    case 2:
        freq = 920000000;  // OFF  ON OFF OFF (KR920)
        break;
    case 3:
        freq = 865000000;  // OFF OFF  ON OFF (IN865)
        break;
    case 4:
        freq = 868000000;  //  ON OFF  ON OFF (EU868)
        break;
    case 5:
        freq = 864000000;  // OFF  ON  ON OFF (RU864)
        break;
    case 6:
        freq = 779000000;  // OFF OFF OFF  ON (CN779)
        break;
    case 7:
        freq = 470000000;  //  ON OFF OFF  ON (CN470)
        break;
    case 8:
        freq = 433000000;  // OFF  ON OFF  ON (EU433)
        break;
    }
    radioSetRFFrequency(freq);
#else
    // When using NUCLEO, use US region
    radioSetRFFrequency(915000000);
#endif
    
    // Init LEDs
    gpio_init_structure.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init_structure.Pull = GPIO_NOPULL;
    gpio_init_structure.Speed = GPIO_SPEED_FREQ_LOW;
    gpio_init_structure.Pin = LED_BLUE_Pin;
    HAL_GPIO_Init(LED_BLUE_GPIO_Port, &gpio_init_structure);
    HAL_GPIO_WritePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin, GPIO_PIN_SET);
    gpio_init_structure.Pin = LED_GREEN_Pin;
    HAL_GPIO_Init(LED_GREEN_GPIO_Port, &gpio_init_structure);
    HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_SET);
    gpio_init_structure.Pin = LED_RED_Pin;
    HAL_GPIO_Init(LED_RED_GPIO_Port, &gpio_init_structure);
    HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_SET);

    // Init button, and determine whether or not it was held down at boot
    gpio_init_structure.Pin = BUTTON1_Pin;
    gpio_init_structure.Mode = GPIO_MODE_IT_RISING_FALLING;
    gpio_init_structure.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio_init_structure.Pull = BUTTON1_ACTIVE_HIGH ? GPIO_PULLDOWN : GPIO_PULLUP;
    HAL_GPIO_Init(BUTTON1_GPIO_Port, &gpio_init_structure);
    for (int i=0; i<250; i++) {
        HAL_Delay(1);
        buttonHeldAtBoot = (HAL_GPIO_ReadPin(BUTTON1_GPIO_Port, BUTTON1_Pin) == (BUTTON1_ACTIVE_HIGH ? GPIO_PIN_SET : GPIO_PIN_RESET));
        if (!buttonHeldAtBoot) {
            break;
        }
    }
    HAL_NVIC_SetPriority(BUTTON1_EXTI_IRQn, BUTTONx_IT_PRIORITY, 0x00);
    HAL_NVIC_EnableIRQ(BUTTON1_EXTI_IRQn);

}

// Unpack bytes into a buffer, LSB to MSB
void unpack32(uint8_t *p, uint32_t value)
{
    p[0] = (value >> 0) & 0xff;
    p[1] = (value >> 8) & 0xff;
    p[2] = (value >> 16) & 0xff;
    p[3] = (value >> 24) & 0xff;
}

// Register the app as a util task
void registerApp(void)
{

    // Determine the gateway address
    if (appIsGateway) {
        memcpy(gatewayAddress, ourAddress, ADDRESS_LEN);
    } else {
        if (!flashConfigFindPeerByType(PEER_TYPE_GATEWAY, gatewayAddress, NULL, NULL)) {
            memcpy(gatewayAddress, invalidAddress, sizeof(gatewayAddress));
        }
    }

    // Initialize the primary task
    if (appIsGateway) {
        appGatewayInit();
        UTIL_SEQ_RegTask((1 << CFG_SEQ_Task_Sparrow_Process), UTIL_SEQ_RFU, appGatewayProcess);
    } else {
        appSensorInit();
        UTIL_SEQ_RegTask((1 << CFG_SEQ_Task_Sparrow_Process), UTIL_SEQ_RFU, appSensorProcess);
    }

}

// Enter SoftAP mode, and wait here until we're no longer in that mode
void appEnterSoftAP(void)
{

    // Get the card status
    J *rsp = NoteRequestResponse(NoteNewRequest("card.status"));
    if (rsp == NULL) {
        return;
    }
    if (NoteResponseError(rsp)) {
        NoteDeleteResponse(rsp);
        return;
    }

    // If it's not a wifi Notecard, we're done
    if (!JGetBool(rsp, "wifi")) {
        NoteDeleteResponse(rsp);
        return;
    }

    // Done with the response
    NoteDeleteResponse(rsp);

    // Enter SoftAP mode
    J *req = NoteNewRequest("card.wifi");
    if (req == NULL) {
        return;
    }
    JAddBoolToObject(req, "start", true);
    NoteRequest(req);

    // Now, stay in this mode for so long as we're
    // in SoftAP mode.
    ledReset();
    while (true) {
        HAL_Delay(750);
        ledWalk();
        J *rsp = NoteRequestResponse(NoteNewRequest("card.wifi"));
        if (rsp == NULL) {
            NoteDeleteResponse(rsp);
            break;
        }
        // If we can't talk to the Notecard during the brief moment
        // that it is rebooting into SoftAP mode, loop.
        if (NoteResponseError(rsp)) {
            NoteDeleteResponse(rsp);
            continue;
        }
        // If we're still in softAP mode, loop
        if (JGetBool(rsp, "start")) {
            NoteDeleteResponse(rsp);
            continue;
        }
        // We're out of softAP mode
        NoteDeleteResponse(rsp);
        break;
    }
    ledReset();

    // We're back with a functioning Notecard

}
