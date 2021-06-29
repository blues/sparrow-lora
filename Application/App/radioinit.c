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

    Radio.Init(&RadioEvents);

#if USE_MODEM_LORA
    atpSetTxConfig();
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

}

// Transmit Timeout ISR
static void OnTxTimeout(void)
{
    Radio.Sleep();
    ledIndicateTransmitInProgress(false);
    appSetCoreState(TX_TIMEOUT);
}

// Receive Timeout ISR
static void OnRxTimeout(void)
{
    wireReceivedLen = 0;
    Radio.Sleep();
    ledIndicateReceiveInProgress(false);
    appSetCoreState(RX_TIMEOUT);
}

// Receive Error ISR
static void OnRxError(void)
{
    wireReceivedLen = 0;
    Radio.Sleep();
    ledIndicateReceiveInProgress(false);
    appSetCoreState(RX_ERROR);
}

// Transmit Completed ISR
static void OnTxDone(void)
{
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

    Radio.Sleep();

    wireReceiveRSSI = rssi;
    wireReceiveSNR = snr;
    wireReceiveSignalValid = true;

    ledIndicateReceiveInProgress(false);
    appSetCoreState(RX);

}
