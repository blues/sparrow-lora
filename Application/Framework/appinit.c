// Copyright 2022 Blues Inc.  All rights reserved.
// Use of this source code is governed by licenses granted by the
// copyright holder including that found in the LICENSE file.

#include "main.h"
#include "stm32_seq.h"
#include "utilities_def.h"
#include "framework.h"

// App Role
bool appIsGateway = false;
int64_t appBootMs = 0;
uint32_t gatewayBootTime = 0;
bool buttonHeldAtBoot = false;

// Which SKU we're configured for
uint32_t SKU = SKU_UNKNOWN;

// Forwards
void registerApp(void);
const char *ioInit(void);
void unpack32(uint8_t *p, uint32_t value);

// Get the version of the image
const char *appFirmwareVersion()
{
    return __DATE__ " " __TIME__;
}

// Main app processing loop
void MX_AppMain(void)
{

    // Initialize GPIOs
    const char *rfsel = ioInit();

    // Determine our own device address
    memset(ourAddress, 0, sizeof(ourAddress));
    unpack32(&ourAddress[0], HAL_GetUIDw0());
    unpack32(&ourAddress[4], HAL_GetUIDw1());
    unpack32(&ourAddress[8], HAL_GetUIDw2());
    utilAddressToText(ourAddress, ourAddressText, sizeof(ourAddressText));

    // Banner
    APP_PRINTF("\r\n");
    APP_PRINTF("===================\r\n");
    APP_PRINTF("===== SPARROW =====\r\n");
    APP_PRINTF("===================\r\n");
    APP_PRINTF("%s\r\n", appFirmwareVersion());
    APP_PRINTF("%s\r\n", ourAddressText);
    APP_PRINTF("%s\r\n", rfsel);
    SKU = SKU_CORE;

    // Remember the time when we were booted
    appBootMs = TIMER_IF_GetTimeMs();

    // Initialize the Notecard
    appIsGateway = noteInit();

    // Conditionally enable or disable trace
    if (appIsGateway) {
        APP_PRINTF("GATEWAY MODE\r\n");
        MX_DBG_Enable();
    } else {
        APP_PRINTF("APPLICATION HOST MODE\r\n");
        if (!buttonHeldAtBoot && !MX_DBG_Active()) {
            APP_PRINTF("CONSOLE TRACE DISABLED\r\n");
            MX_DBG_Disable();
        } else {
            MX_DBG_Enable();
            APP_PRINTF("CONSOLE TRACE ENABLED\r\n");
        }
    }

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
        APP_PRINTF("Waiting for time from Notecard\r\n");
        ledReset();
        while ( !NoteTimeValidST() || !gatewayEnvVarsLoaded() ) {
            if (ledButtonCheck() == BUTTON_HELD) {
                flashConfigFactoryReset();
            }
            ledWalk();
            HAL_Delay(750);
        }
        APP_PRINTF("Waiting up to 30 sec for time zone from Notecard\r\n");
        for (int i=0; i<60 && !NoteRegion(NULL, NULL, NULL, NULL); i++) {
            ledWalk();
            HAL_Delay(500);
        }
        gatewayBootTime = NoteTimeST();
        APP_PRINTF("Time: %d\r\n", gatewayBootTime);
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
        appButtonWakeup();
        GPIO_Pin &= ~BUTTON1_Pin;
        if (GPIO_Pin == 0) {
            return;
        }
    }

    // Dispatch the interrupts to all sensors
    if (!appIsGateway) {
        schedDispatchISR(GPIO_Pin);
    }

}

// Get the digital state of a GPIO, leaving it in analog mode
int pinstate(void *portv, uint16_t pin)
{
    GPIO_TypeDef *port = (GPIO_TypeDef *) portv;
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
        return PINSTATE_FLOAT;
    }
    return (high ? PINSTATE_HIGH : PINSTATE_LOW);
}

// Initialize app hardware I/O
const char *ioInit(void)
{
    GPIO_InitTypeDef  gpio_init_structure = {0};

#if RF_FREQ != 0
    // Respect the RF frequency explicitly selected
    // using `RF_FREQ` from `../config_radio.h`
    uint32_t freq = RF_FREQ;
#else
    uint32_t freq = 915000000;
#if defined(USE_SPARROW) && defined(USE_LED_TX)
    // Compute the RF frequency based on the region switch settings.  Note that
    // we power these pins with LED_TX (LED_RED on the schematic) so that even
    // if the user happens to select an invalid switch combination they aren't
    // a constant current draw on the system. This switch design methodology
    // allows for a selection of any of 9 unique frequency plans based on the
    // switches.  We have chosen what we view to be the most common plans
    // globally, but the developer can feel free to reassign these as is
    // appropriate for their product or market.
    gpio_init_structure.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init_structure.Pull = GPIO_NOPULL;
    gpio_init_structure.Speed = GPIO_SPEED_FREQ_LOW;
    gpio_init_structure.Pin = LED_TX_Pin;
    HAL_GPIO_Init(LED_TX_GPIO_Port, &gpio_init_structure);
    HAL_GPIO_WritePin(LED_TX_GPIO_Port, LED_TX_Pin, GPIO_PIN_SET);
    int gpio0 = pinstate(RFSEL_1_GPIO_Port, RFSEL_1_Pin);
    int gpio1 = pinstate(RFSEL_0_GPIO_Port, RFSEL_0_Pin);
    char *sw = "(unknown switch positions)";
    if (gpio0 == PINSTATE_FLOAT && gpio1 == PINSTATE_FLOAT) {
        sw = "0 OFF OFF OFF OFF US915";
        freq = 915000000;   // US915
    } else if (gpio0 == PINSTATE_HIGH && gpio1 == PINSTATE_FLOAT) {
        sw = "1  ON OFF OFF OFF AS923";
        freq = 923000000;   // AS923
    } else if (gpio0 == PINSTATE_LOW && gpio1 == PINSTATE_FLOAT) {
        sw = "2 OFF  ON OFF OFF KR920";
        freq = 920000000;   // KR920
    } else if (gpio0 == PINSTATE_FLOAT && gpio1 == PINSTATE_HIGH) {
        sw = "3 OFF OFF  ON OFF IN865";
        freq = 865000000;   // IN865
    } else if (gpio0 == PINSTATE_HIGH && gpio1 == PINSTATE_HIGH) {
        sw = "4 ON OFF  ON OFF EU868";
        freq = 868000000;   // EU868
    } else if (gpio0 == PINSTATE_LOW && gpio1 == PINSTATE_HIGH) {
        sw = "5 OFF  ON  ON OFF RU864";
        freq = 864000000;   // RU864
    } else if (gpio0 == PINSTATE_FLOAT && gpio1 == PINSTATE_LOW) {
        sw = "6 OFF OFF OFF  ON AU915";
        freq = 915000000;   // AU915
    } else if (gpio0 == PINSTATE_HIGH && gpio1 == PINSTATE_LOW) {
        sw = "7 ON OFF OFF  ON CN470";
        freq = 470000000;   // CN470
//        freq = 779000000;   // CN779
    } else if (gpio0 == PINSTATE_LOW && gpio1 == PINSTATE_LOW) {
        sw = "8 OFF  ON OFF  ON EU433";
        freq = 433000000;   // EU433
    }
#ifdef DEBUG_RFSEL
    char *s0 = (gpio0 == PINSTATE_FLOAT ? "float" : (gpio0 == PINSTATE_HIGH ? "high" : "low"));
    char *s1 = (gpio1 == PINSTATE_FLOAT ? "float" : (gpio1 == PINSTATE_HIGH ? "high" : "low"));
    APP_PRINTF("*** rfsel %s %s %dMHz ***\r\n", s0, s1, (freq/1000000));
#endif
#endif
#endif
    radioSetRFFrequency(freq);

    // Init LEDs
    gpio_init_structure.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init_structure.Pull = GPIO_NOPULL;
    gpio_init_structure.Speed = GPIO_SPEED_FREQ_LOW;
#ifdef USE_LED_PAIR
    gpio_init_structure.Pin = LED_PAIR_Pin;
    HAL_GPIO_Init(LED_PAIR_GPIO_Port, &gpio_init_structure);
    HAL_GPIO_WritePin(LED_PAIR_GPIO_Port, LED_PAIR_Pin, LED_PAIR_ON);
#endif
#ifdef USE_LED_RX
    gpio_init_structure.Pin = LED_RX_Pin;
    HAL_GPIO_Init(LED_RX_GPIO_Port, &gpio_init_structure);
    HAL_GPIO_WritePin(LED_RX_GPIO_Port, LED_RX_Pin, LED_RX_ON);
#endif
#ifdef USE_LED_TX
    gpio_init_structure.Pin = LED_TX_Pin;
    HAL_GPIO_Init(LED_TX_GPIO_Port, &gpio_init_structure);
    HAL_GPIO_WritePin(LED_TX_GPIO_Port, LED_TX_Pin, LED_TX_ON);
#endif

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

    // Return rfsel
    return sw;

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

// Set the SKU
void appSetSKU(int sku)
{
    SKU = sku;
}

// Get the SKU
int appSKU(void)
{
    return SKU;
}
