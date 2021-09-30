// Copyright 2021 Blues Inc.  All rights reserved.
// Use of this source code is governed by licenses granted by the
// copyright holder including that found in the LICENSE file.

#include "main.h"
#include "app.h"
#include "radio.h"

// Global radio data
bool wireReceiveSignalValid = false;
int8_t wireReceiveRSSI = 0;
int8_t wireReceiveSNR = 0;
int8_t wireTransmitDb = 0;
bool radioIsDeepSleep = false;
bool radioIOPending = false;

/* Radio events function pointer */
static RadioEvents_t RadioEvents;
static void OnTxDone(void);
static void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr);
static void OnTxTimeout(void);
static void OnRxTimeout(void);
static void OnRxError(void);

// Initialize the radio
void radioInit()
{

    // Ensure that our scheduler knows that we're awake
    radioIsDeepSleep = false;

    uint16_t sizeWM = sizeof(wireMessage);
    uint16_t sizeWMC = sizeof(wireMessageCarrier);
    if (sizeWM > 254 || sizeWMC > 254) {
        traceLn("*** maximum LoRa message size is 254 ***");
    }

    RadioEvents.TxDone = OnTxDone;
    RadioEvents.RxDone = OnRxDone;
    RadioEvents.TxTimeout = OnTxTimeout;
    RadioEvents.RxTimeout = OnRxTimeout;
    RadioEvents.RxError = OnRxError;

    radioIOPending = false;
    Radio.Init(&RadioEvents);

#if USE_MODEM_LORA
    radioSetTxPower(atpPowerLevel());
    Radio.SetRxConfig(MODEM_LORA,
                      LORA_BANDWIDTH,
                      LORA_SPREADING_FACTOR,
                      LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
                      LORA_SYMBOL_TIMEOUT, LORA_FIX_LENGTH_PAYLOAD_ON,
                      0, true, 0, 0, LORA_IQ_INVERSION_ON, true);
    Radio.SetMaxPayloadLength(MODEM_LORA, sizeof(wireMessageCarrier));
#endif

#if USE_MODEM_FSK
    Radio.SetTxConfig(MODEM_FSK, TP_DEFAULT, FSK_FDEV, 0,
                      FSK_DATARATE, 0,
                      FSK_PREAMBLE_LENGTH, FSK_FIX_LENGTH_PAYLOAD_ON,
                      true, 0, 0, 0, TX_TIMEOUT_VALUE);
    Radio.SetRxConfig(MODEM_FSK, FSK_BANDWIDTH, FSK_DATARATE,
                      0, FSK_AFC_BANDWIDTH, FSK_PREAMBLE_LENGTH,
                      0, FSK_FIX_LENGTH_PAYLOAD_ON, 0, true,
                      0, 0, false, true);
    Radio.SetMaxPayloadLength(MODEM_FSK, sizeof(wireMessageCarrier));
#endif

    wireTransmitDb = 0;

}

// De-initialize the radio
void radioDeInit()
{
    Radio.DeInit();
    radioIsDeepSleep = true;
}

// Place the radio in deep-sleep mode if no I/O is pending
bool radioDeepSleep()
{
    if (radioIsDeepSleep) {
        return false;
    }
    if (radioIOPending) {
        return false;
    }
    Radio.DeepSleep();
    radioIsDeepSleep = true;
    return true;
}

// Wake the radio from deep sleep if we're sleeping
void radioDeepWake()
{
    if (radioIsDeepSleep) {
        radioInit();
        HAL_Delay(500);
    }
}

// Transmit Timeout ISR
static void OnTxTimeout(void)
{
    radioIOPending = false;
    Radio.Sleep();
    ledIndicateTransmitInProgress(false);
    appSetCoreState(TX_TIMEOUT);
}

// Receive Timeout ISR
static void OnRxTimeout(void)
{
    wireReceivedLen = 0;
    radioIOPending = false;
    Radio.Sleep();
    ledIndicateReceiveInProgress(false);
    appSetCoreState(RX_TIMEOUT);
}

// Receive Error ISR
static void OnRxError(void)
{
    wireReceivedLen = 0;
    radioIOPending = false;
    Radio.Sleep();
    ledIndicateReceiveInProgress(false);
    appSetCoreState(RX_ERROR);
}

// Transmit Completed ISR
static void OnTxDone(void)
{
    radioIOPending = false;
    Radio.Sleep();
    ledIndicateTransmitInProgress(false);
    appSetCoreState(TX);
}

// Receive Completed ISR
static void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr)
{

    if (size > sizeof(wireMessageCarrier)) {
        wireReceivedLen = 0;
    } else {
        wireReceivedLen = size;
        memcpy(&wireReceivedCarrier, payload, size);
    }

    radioIOPending = false;
    Radio.Sleep();

    wireReceiveRSSI = rssi;
    wireReceiveSNR = snr;
    wireReceiveSignalValid = true;

    ledIndicateReceiveInProgress(false);
    appSetCoreState(RX);

}

// Set the channel for transmit or receive
void radioSetChannel(uint32_t frequency)
{
    Radio.SetChannel(frequency);
}

// Get the amount of time necessary to come out of sleep
uint32_t radioWakeupRequiredMs()
{
    return (Radio.GetWakeupTime() + TCXO_WORKAROUND_TIME_MARGIN);
}

// Start a receive
void radioRx(uint32_t timeoutMs)
{
    radioDeepWake();
    Radio.Rx(timeoutMs);
    radioIOPending = true;
}

// Transmit
void radioTx(uint8_t *buffer, uint8_t size)
{
    radioDeepWake();
    Radio.Send(buffer, size);
    radioIOPending = true;
}

// Set last known tx power to unknown
void radioSetTxPowerUnknown()
{
    wireTransmitDb = 0;
}

// Set tx power
void radioSetTxPower(int8_t powerLevel)
{
    wireTransmitDb = powerLevel;
    Radio.SetTxConfig(MODEM_LORA,
                      powerLevel,                   // output power in dBm
                      0,                            // unused for LoRa
                      LORA_BANDWIDTH,
                      LORA_SPREADING_FACTOR,
                      LORA_CODINGRATE,
                      LORA_PREAMBLE_LENGTH,
                      LORA_FIX_LENGTH_PAYLOAD_ON,
                      true,                         // CRC on/off
                      0,                            // Frequency hopping off/on
                      0,                            // # of symbols between hops
                      LORA_IQ_INVERSION_ON,         // Invert IQ signal
                      TX_TIMEOUT_VALUE);            // Timeout on radio.Send()
}
