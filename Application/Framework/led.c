// Copyright 2022 Blues Inc.  All rights reserved.
// Use of this source code is governed by licenses granted by the
// copyright holder including that found in the LICENSE file.

#include "main.h"
#include "framework.h"

// State
uint32_t walkState = 0;
bool ledStatePair = false;
bool ledStatePairTimeWasValid = false;
uint32_t ledStatePairBeganTime = 0;
bool ledStateReceive = false;
bool ledStateTransmit = false;

// On sensor, enable/disabled for battery savings
#define ledsEnabledMins 15
int64_t ledsEnabledMs = 0;

// Return TRUE if the LEDs should be suppressed for power reasons
bool ledDisabled()
{

#if !LED_ALWAYS

    // Never suppress if this is a gateway role
    if (appIsGateway) {
        return false;
    }

    // Exit if not yet paired
    if (ledIsPairInProgress() || ledIsPairMandatory()) {
        return false;
    }

    // Never suppress if we're in trace mode
    if (MX_DBG_Enabled()) {
        return false;
    }

    // Suppress if within the window
    if (ledsEnabledMs == 0) {
        ledsEnabledMs = TIMER_IF_GetTimeMs();
    }
    uint32_t ledDisableAtMs = ledsEnabledMs + (ledsEnabledMins * 60 * 1000);
    if (TIMER_IF_GetTimeMs() >= ledDisableAtMs) {
        return true;
    }

#endif

    // Allow them to be turned on/off
    return false;

}

// Initialize the LEDs
void ledSet()
{
#ifdef USE_LED_PAIR
    HAL_GPIO_WritePin(LED_PAIR_GPIO_Port, LED_PAIR_Pin, LED_PAIR_ON);
#endif
#ifdef USE_LED_RX
    HAL_GPIO_WritePin(LED_RX_GPIO_Port, LED_RX_Pin, LED_RX_ON);
#endif
#ifdef USE_LED_TX
    HAL_GPIO_WritePin(LED_TX_GPIO_Port, LED_TX_Pin, LED_TX_ON);
#endif
}

// Initialize the LEDs
void ledReset()
{
    walkState = 0;
#ifdef USE_LED_PAIR
    HAL_GPIO_WritePin(LED_PAIR_GPIO_Port, LED_PAIR_Pin, LED_PAIR_OFF);
#endif
#ifdef USE_LED_RX
    HAL_GPIO_WritePin(LED_RX_GPIO_Port, LED_RX_Pin, LED_RX_OFF);
#endif
#ifdef USE_LED_TX
    HAL_GPIO_WritePin(LED_TX_GPIO_Port, LED_TX_Pin, LED_TX_OFF);
#endif
}

// Begin an LED walk
void ledWalk()
{
    uint32_t c = (walkState++) % 4;
#ifdef USE_LED_PAIR
    HAL_GPIO_WritePin(LED_PAIR_GPIO_Port, LED_PAIR_Pin, c == 0 ? LED_PAIR_ON : LED_PAIR_OFF);
#endif
#ifdef USE_LED_RX
    HAL_GPIO_WritePin(LED_RX_GPIO_Port, LED_RX_Pin, c == 1 || c == 3 ? LED_RX_ON : LED_RX_OFF);
#endif
#ifdef USE_LED_TX
    HAL_GPIO_WritePin(LED_TX_GPIO_Port, LED_TX_Pin, c == 2 ? LED_TX_ON : LED_TX_OFF);
#endif
}

// Is in progress?
bool ledIsPairMandatory()
{
    return (memcmp(gatewayAddress, invalidAddress, sizeof(gatewayAddress)) == 0);
}

// Is in progress?
bool ledIsPairInProgress()
{

    // Time-out the pairing
    if (ledStatePairBeganTime > 0) {
        if (!ledStatePairTimeWasValid && NoteTimeValidST()) {
            ledStatePairBeganTime = NoteTimeST();
        }
        uint32_t timeoutSecs = 60*(appIsGateway ? var_gateway_pairing_timeout_mins : PAIRING_BEACON_SENSOR_TIMEOUT_MINS);
        if (NoteTimeST() > ledStatePairBeganTime + timeoutSecs) {
            ledIndicatePairInProgress(false);
        }
    }

    // If gateway, the answer is simple
    if (appIsGateway) {
        return ledStatePair;
    }

    // If a sensor that's never been paired, always put us into that mode.  Note that
    // this behavior is optional, because the net-effect of this code is that if you
    // insert batteries into multiple devices, you won't be able to distinguish them
    // when they start pairing with the gateway.
#ifdef AUTO_PAIR_IF_UNPAIRED
    if (ledStatePairBeganTime == 0 && ledIsPairMandatory()) {
        ledIndicatePairInProgress(true);
    }
#endif

    // Done
    return ledStatePair;

}

// Indicate that a pairing is in progress
void ledIndicatePairInProgress(bool on)
{
    ledStatePair = on;
    ledStatePairBeganTime = on ? NoteTimeST() : 0;
    ledStatePairTimeWasValid = NoteTimeValidST();
#ifdef USE_LED_PAIR
    HAL_GPIO_WritePin(LED_PAIR_GPIO_Port, LED_PAIR_Pin, on ? LED_PAIR_ON : LED_PAIR_OFF);
#endif
    APP_PRINTF("%s\r\n", on ? "pairing mode ON" : "pairing mode OFF");
}

// Is in progress?
bool ledIsReceiveInProgress()
{
    return ledStateReceive;
}

// Indicate that a receive is in progress
void ledIndicateReceiveInProgress(bool on)
{
#ifdef USE_LED_RX
    ledStateReceive = on;
    if (ledDisabled()) {
        HAL_GPIO_WritePin(LED_RX_GPIO_Port, LED_RX_Pin, LED_RX_OFF);
    } else {
        HAL_GPIO_WritePin(LED_RX_GPIO_Port, LED_RX_Pin, on ? LED_RX_ON : LED_RX_OFF);
    }
#endif
}

// Is in progress?
bool ledIsTransmitInProgress()
{
    return ledStateTransmit;
}

// Indicate that a transmit is in progress
void ledIndicateTransmitInProgress(bool on)
{
#ifdef USE_LED_TX
    ledStateTransmit = on;
    if (ledDisabled()) {
        HAL_GPIO_WritePin(LED_TX_GPIO_Port, LED_TX_Pin, LED_TX_OFF);
    } else {
        HAL_GPIO_WritePin(LED_TX_GPIO_Port, LED_TX_Pin, on ? LED_TX_ON : LED_TX_OFF);
    }
#endif
}

// Indicate OK
void ledIndicateAck(int flashes)
{
    for (int i=0; i<flashes; i++) {
        ledSet();
        HAL_Delay(250);
        ledReset();
        HAL_Delay(250);
    }
}

// Toggle LEDs with a pattern so long as a given button is being held down
uint16_t ledButtonCheck()
{

    // Exit if button isn't even being pressed
    if (HAL_GPIO_ReadPin(BUTTON1_GPIO_Port, BUTTON1_Pin) == (BUTTON1_ACTIVE_HIGH ? GPIO_PIN_RESET : GPIO_PIN_SET)) {
        return BUTTON_UNCHANGED;
    }

    // Enable LEDs for some period of time after the button has been pressed
    ledsEnabledMs = TIMER_IF_GetTimeMs();

    // Wait until released
#ifdef USE_LED_TX
    bool txWasOn = (LED_TX_ON == HAL_GPIO_ReadPin(LED_TX_GPIO_Port, LED_TX_Pin));
#endif
#ifdef USE_LED_RX
    bool rxWasOn = (LED_RX_ON == HAL_GPIO_ReadPin(LED_RX_GPIO_Port, LED_RX_Pin));
#endif
#ifdef USE_LED_PAIR
    bool pairWasOn = (LED_PAIR_ON == HAL_GPIO_ReadPin(LED_PAIR_GPIO_Port, LED_PAIR_Pin));
#endif
    int flashes = 0;
    uint32_t beganSecs = NoteTimeST();
    uint32_t expireSecs = 15;
    uint32_t currentDelayMs = 750;
    uint32_t prevQuartile = 0;
    while (NoteTimeST() < beganSecs+expireSecs) {
        if (HAL_GPIO_ReadPin(BUTTON1_GPIO_Port, BUTTON1_Pin) == (BUTTON1_ACTIVE_HIGH ? GPIO_PIN_RESET : GPIO_PIN_SET)) {
#ifdef USE_LED_TX
            HAL_GPIO_WritePin(LED_TX_GPIO_Port, LED_TX_Pin, txWasOn ? LED_TX_ON : LED_TX_OFF);
#endif
#ifdef USE_LED_RX
            HAL_GPIO_WritePin(LED_RX_GPIO_Port, LED_RX_Pin, rxWasOn ? LED_RX_ON : LED_RX_OFF);
#endif
#ifdef USE_LED_PAIR
            HAL_GPIO_WritePin(LED_PAIR_GPIO_Port, LED_PAIR_Pin, pairWasOn ? LED_PAIR_ON : LED_PAIR_OFF);
#endif
            return (flashes < 2) ? BUTTON_PRESSED : BUTTON_HOLD_ABORTED;
        }
        if (flashes >= 1) {
#ifdef USE_LED_TX
            HAL_GPIO_WritePin(LED_TX_GPIO_Port, LED_TX_Pin, (flashes & 1) ? LED_TX_ON : LED_TX_OFF);
#endif
#ifdef USE_LED_RX
            HAL_GPIO_WritePin(LED_RX_GPIO_Port, LED_RX_Pin, (flashes & 1) ? LED_RX_ON : LED_RX_OFF);
#endif
#ifdef USE_LED_PAIR
            HAL_GPIO_WritePin(LED_PAIR_GPIO_Port, LED_PAIR_Pin, (flashes & 1) ? LED_PAIR_ON : LED_PAIR_OFF);
#endif
        }
        uint32_t elapsed = NoteTimeST() - beganSecs;
        uint32_t quartile = elapsed / (expireSecs/4);
        if (prevQuartile != quartile) {
            prevQuartile = quartile;
            currentDelayMs -= currentDelayMs < 250 ? 0 : 200;
        }
        HAL_Delay(currentDelayMs);
        flashes++;
    }

    // Held down for a while
    return BUTTON_HELD;

}
