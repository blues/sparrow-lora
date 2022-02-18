// Copyright 2022 Blues Inc.  All rights reserved.
// Use of this source code is governed by licenses granted by the
// copyright holder including that found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "stm32_timer.h"
#include "stm32_seq.h"
#include "utilities_def.h"
#include "main.h"
#include "framework.h"

// Task state
States_t CurrentStateCore = LOWPOWER;
bool ListenPhaseBeforeTalk = false;
bool ButtonEventOccurred = false;
bool TimerEventOccurred = false;
bool TraceEventOccurred = false;

// Running sequence of request IDs issued to the gateway
uint32_t LastRequestID = 0;

// Addresses and Keys
uint8_t wildcardAddress[ADDRESS_LEN] = {0};
uint8_t invalidAddress[ADDRESS_LEN] = {0};
uint8_t ourAddress[ADDRESS_LEN];
char ourAddressText[ADDRESS_LEN*3];
uint8_t lastReceivedSensorAddress[ADDRESS_LEN] = {0};
uint8_t gatewayAddress[ADDRESS_LEN] = {0};
uint8_t beaconKey[AES_KEY_BYTES];
uint8_t invalidKey[AES_KEY_BYTES] = {0};
char sensorName[SENSOR_NAME_MAX] = {0};

// Battery management
uint16_t batteryMillivolts = 0;

// Time Windowing
uint16_t twLBTRetries = 10;
uint16_t twLBTRetriesRemaining;
uint32_t twLastActiveSensors = 0;
uint32_t TWModulusSecs = 0;
uint16_t TWModulusOffsetSecs = 0;
uint16_t TWSlotBeginsSecs = 0;
bool TWSlotBeginsTweak = false;
uint16_t TWSlotEndsSecs = 0;
uint16_t TWListenBeforeTalkMs = 0;
uint32_t twSlotBeginsTime;
uint32_t twSlotExpiresTime;
bool twForceIgnore = false;
bool twSlotExpiresTimeWasValid;
static UTIL_TIMER_Object_t twSleepTimer;

// Sent message state
uint32_t sensorSendRetriesRemaining;
uint32_t messageToSendRequestID;
uint8_t messageToSendFlags;
uint8_t messageToSendRSSI;
uint8_t messageToSendSNR;
uint8_t *messageToSendData;
uint32_t messageToSendDataLen;
bool messageToSendDataDealloc;
uint32_t messageToSendAcknowledgedLen;
int64_t sentMessageMs;
uint16_t sentMessageCarrierLen;
wireMessageCarrier sentMessageCarrier;
wireMessage sentMessage;

// Received message state
wireMessageCarrier wireReceivedCarrier;
wireMessage wireReceived;
uint32_t wireReceivedLen;
uint32_t wireReceiveTimeoutMs;

// Gateway's per-sensor request state
typedef struct {
    bool receivingRequest;
    bool sendingResponse;
    bool responseRequired;
    int8_t gatewayRSSI;
    int8_t gatewaySNR;
    int8_t sensorRSSI;
    int8_t sensorSNR;
    int8_t sensorTXP;
    int8_t sensorLTP;
    uint16_t sensorMv;
    uint16_t twSlotBeginsSecs;
    uint16_t twSlotEndsSecs;
    uint32_t lastReceivedTime;
    uint8_t sensorAddress[ADDRESS_LEN];
    uint32_t currentRequestID;
    uint32_t lastProcessedRequestID;
    uint32_t lastProcessedRequestIDForAck;
    uint32_t requestsProcessed;
    uint32_t requestsLost;
    uint8_t *data;
    uint32_t dataTotalLen;
    uint32_t dataAcknowledgedLen;
} requestState;
requestState requestCache[MAX_CACHED_SENSORS] = {0};
uint8_t cachedSensors = 0;

// Sensor database update info
bool forceSensorRefresh = false;

// Sensor's response state when communicating with gateway
typedef struct {
    bool sendingRequest;
    bool receivingResponse;
    bool responseRequired;
    uint32_t requestID;
    uint8_t *data;
    uint32_t dataTotalLen;
    uint32_t dataAcknowledgedLen;
    bool requestFullySent;
} responseState;
responseState response = {0};

// Forwards
void gatewayWaitForSensorMessage(void);
void gatewayWaitForAnySensorMessage(void);
void sensorWaitForGatewayMessage(void);
void sensorWaitForGatewayResponse(void);
void sensorSendToGateway(bool responseRequested, uint8_t *message, uint32_t length, bool dealloc);
bool sensorResendToGateway(void);
void sendToPeer(bool useTW, uint8_t flags, int8_t rssi, int8_t snr, uint8_t *toAddress, uint32_t requestID,
                uint8_t *message, uint32_t length, bool dealloc);
void sendMessageToPeer(bool useTW, uint8_t *toAddress);
bool sendTimeout(void);
void freeMessageToSendBuffer(void);
void restartReceive(uint32_t timeoutMs);
bool validateReceivedMessage(void);
void processSensorRequest(requestState *request, bool respond);
bool lbtListenBeforeTalk(void);
void lbtTalk(void);
void twRefresh(void);
void twOpenEvent(void *context);
uint32_t twMinimumModulusSecs(void);
void sensorCoreIdle(void);
void sensorGatewayRequestFailure(bool wasTX, const char *why);
void showReceivedTime(char *msg, uint32_t beginSecs, uint32_t endSecs);

// Set the current application state, potentially from an ISR
void appSetCoreState(States_t newState)
{

#ifdef TRACE_STATE
    APP_PRINTF("SET %d\r\n", newState);
#endif

    // Set the application-level state for the next time we're scheduled
    CurrentStateCore = newState;

    // Wake up the scheduler
    if (newState != LOWPOWER) {
        UTIL_SEQ_SetTask((1 << CFG_SEQ_Task_Sparrow_Process), CFG_SEQ_Prio_0);
    }

}

// Wake up the main task for timer processing
void appTraceWakeup()
{
    TraceEventOccurred = true;
    UTIL_SEQ_SetTask((1 << CFG_SEQ_Task_Sparrow_Process), CFG_SEQ_Prio_0);
}

// Wake up the main task for timer processing
void appTimerWakeup()
{
    TimerEventOccurred = true;
    UTIL_SEQ_SetTask((1 << CFG_SEQ_Task_Sparrow_Process), CFG_SEQ_Prio_0);
}

// Wake up the main task for button processing
void appButtonWakeup()
{
    ButtonEventOccurred = true;
    UTIL_SEQ_SetTask((1 << CFG_SEQ_Task_Sparrow_Process), CFG_SEQ_Prio_0);
}

// Free the send buffer
void freeMessageToSendBuffer(void)
{

    // Free the previously-allocated buffer
    if (messageToSendDataDealloc) {
        memset(messageToSendData, '?', messageToSendDataLen);
        free(messageToSendData);
    }
    messageToSendData = NULL;
    messageToSendDataLen = 0;
    messageToSendDataDealloc = false;
    messageToSendAcknowledgedLen = 0;

}

// Send a multi-segment message to the peer, with a flag indicating whether or not
// the data should be deallocated (freed) at the end of message transmission. If this
// is false, it is up to the caller to know when is the appropriate time to deallocate
// the "message" buffer.
void sendToPeer(bool useTW, uint8_t flags, int8_t rssi, int8_t snr, uint8_t *toAddress, uint32_t requestID,
                uint8_t *message, uint32_t length, bool dealloc)
{

    // Initialize for a multi-chunk transfer
    freeMessageToSendBuffer();
    messageToSendData = message;
    messageToSendDataLen = length;
    messageToSendDataDealloc = dealloc;
    messageToSendAcknowledgedLen = 0;
    messageToSendRequestID = requestID;
    messageToSendFlags = flags;
    messageToSendRSSI = rssi;
    messageToSendSNR = snr;

    // Send the next chunk
    sendMessageToPeer(useTW, toAddress);

}

// Send the next message in sequence to the peer.  If key is NULL, it is looked up.
void sendMessageToPeer(bool useTW, uint8_t *toAddress)
{

    // Format the header for the next chunk
    sentMessageCarrier.Version = MESSAGE_VERSION;
    sentMessageCarrier.Algorithm = ((messageToSendFlags & MESSAGE_FLAG_BEACON) != 0) ? MESSAGE_ALG_CLEAR : MESSAGE_ALG_CTR;
    sentMessage.Signature = MESSAGE_SIGNATURE;
    sentMessage.Millivolts = batteryMillivolts;
    sentMessage.TXP = atpPowerLevel();
    sentMessage.LTP = atpLowestPowerLevel();
    sentMessage.RSSI = messageToSendRSSI;
    sentMessage.SNR = messageToSendSNR;
    sentMessage.Flags = messageToSendFlags;
    sentMessage.RequestID = messageToSendRequestID;
    uint32_t left = messageToSendDataLen - messageToSendAcknowledgedLen;
    if (messageToSendAcknowledgedLen > messageToSendDataLen) {
        left = 0;
    }
    sentMessage.Offset = messageToSendAcknowledgedLen;
    sentMessage.Len = (left <= MESSAGE_MAX_BODY) ? (uint16_t) left : MESSAGE_MAX_BODY;
    sentMessage.TotalLen = messageToSendDataLen;
    memcpy(sentMessageCarrier.Sender, ourAddress, sizeof(sentMessageCarrier.Sender));
    memcpy(sentMessageCarrier.Receiver, toAddress, sizeof(sentMessageCarrier.Receiver));
    if (sentMessage.Len) {
        memcpy(sentMessage.Body, &messageToSendData[messageToSendAcknowledgedLen], sentMessage.Len);
    }

    const char *m1 = "sending (";
    DEBUG_VARIABLE(m1);
    if ((messageToSendFlags & MESSAGE_FLAG_ACK)) {
        m1 = "sending ACK";
    } else if ((messageToSendFlags & MESSAGE_FLAG_BEACON)) {
        m1 = "sending BEACON";
    }
    APP_PRINTF("%s %s (%d/%d) at txp:%d\r\n", tracePeer(), m1, sentMessage.Len, messageToSendDataLen, atpPowerLevel());

    // Compute message length of actual message
    uint16_t wireMessageLen = sizeof(sentMessage);
    wireMessageLen -= sizeof(sentMessage.Padding);
    wireMessageLen -= sizeof(sentMessage.Body);
    wireMessageLen += sentMessage.Len;
    uint16_t padRequired = (wireMessageLen % AES_PAD_BYTES) == 0 ? 0 : AES_PAD_BYTES - (wireMessageLen % AES_PAD_BYTES);
    sentMessageCarrier.MessageLen = wireMessageLen + padRequired;
    sentMessageCarrierLen = sizeof(sentMessageCarrier);
    sentMessageCarrierLen -= sizeof(sentMessageCarrier.Message);
    sentMessageCarrierLen += sentMessageCarrier.MessageLen;
    // Pad the body with data to fill out to AES block size
    for (int i=0; i<padRequired; i++) {
        sentMessage.Body[sentMessage.Len+i] = i;
    }

    // See if encryption is necessary
    if (sentMessageCarrier.Algorithm == MESSAGE_ALG_CLEAR) {

        sentMessageCarrier.Message = sentMessage;

    } else {

        // Always use the sensor's key when encrypting
        uint8_t key[AES_KEY_BYTES];
        uint8_t *sensorAddress = appIsGateway ? sentMessageCarrier.Receiver : sentMessageCarrier.Sender;
        if (!flashConfigFindPeerByAddress(sensorAddress, NULL, key, NULL)) {
            APP_PRINTF("can't find the sensor's key\r\n");
            memcpy(key, invalidKey, sizeof(key));
        }

        // Trace
#ifdef SHOW_KEYS
        APP_PRINTF("ENCRYPT: ");
        for (int i=0; i<ADDRESS_LEN; i++) {
            APP_PRINTF("%02x", sensorAddress[i]);
        }
        APP_PRINTF(": ");
        for (int i=0; i<sizeof(key); i++) {
            APP_PRINTF("%02x", key[i]);
        }
        APP_PRINTF("\r\n");
#endif

        // Encrypt the data
        bool success = MX_AES_CTR_Encrypt(key, (uint8_t *)&sentMessage, sentMessageCarrier.MessageLen, (uint8_t *)&sentMessageCarrier.Message);
        memcpy(key, invalidKey, sizeof(key));
        if (!success) {
            APP_PRINTF("encryption error\r\n");
        }

    }

    // If this is the gateway, just send it
    if (!useTW) {

        // We've had some issues in which the sensor has not put itself into
        // receive mode quickly enough, and our reply got there too soon.
        // This gives some breathing room.  Note that we don't need
        // to do this if we're using LBT because the LBT delay is sufficient.
        if (RADIO_TURNAROUND_ALLOWANCE_MS != 0) {
            HAL_Delay(RADIO_TURNAROUND_ALLOWANCE_MS);
        }

        // Send the packet now
        lbtTalk();

        return;
    }

    // Compute the next slot
    uint32_t sleepSecs = appNextTransmitWindowDueSecs();
    APP_PRINTF("%s waiting %ds to transmit (slot %ds-%ds in %ds window)\r\n",
               tracePeer(), sleepSecs, TWSlotBeginsSecs, TWSlotEndsSecs, TWModulusSecs);

    // Schedule the timer for the next open transmit window
    ledIndicateTransmitInProgress(false);
    ledIndicateReceiveInProgress(true);
    HAL_Delay(100);
    ledIndicateReceiveInProgress(false);
    ledIndicateTransmitInProgress(true);
    HAL_Delay(100);
    ledIndicateTransmitInProgress(false);
    UTIL_TIMER_Create(&twSleepTimer, 0xFFFFFFFFU, UTIL_TIMER_ONESHOT, twOpenEvent, NULL);
    UTIL_TIMER_SetPeriod(&twSleepTimer, (sleepSecs*1000)+1);
    UTIL_TIMER_Start(&twSleepTimer);

    // Wait
    appSetCoreState(LOWPOWER);

}

// Get stats relating to last wire message received, both from our perspective and the remote perspective
void appReceivedMessageStats(int8_t *gtxdb, int8_t *grssi, int8_t *grsnr, int8_t *stxdb, int8_t *srssi, int8_t *srsnr)
{
    *stxdb = wireReceived.TXP;
    *srssi = wireReceived.RSSI;
    *srsnr = wireReceived.SNR;
    *gtxdb = wireTransmitDb;
    *grssi = wireReceiveRSSI;
    *grsnr = wireReceiveSNR;
}

// Send a beacon for sensor pairing
void appSendBeaconToGateway()
{

    // Set sensor response state
    response.sendingRequest = true;
    response.receivingResponse = false;

    // The request ID is the decryption algorithm to be used, and the body is the key
    uint32_t requestID = MESSAGE_ALG_CTR;
    uint32_t length = AES_KEY_BYTES;
    MX_RNG_Init();
    for (size_t i=0; i<length; i++) {
        beaconKey[i] = MX_RNG_Get();
    }
    MX_RNG_DeInit();

    // Assign the next request ID, which is used to determine packet loss
    traceSetID("BE", wildcardAddress, requestID);

    // Send it
    sendToPeer(false, MESSAGE_FLAG_BEACON, wireReceiveRSSI, wireReceiveSNR, wildcardAddress, requestID,
               beaconKey, sizeof(beaconKey), false);

}

// Process the timed event
void twOpenEvent(void *context)
{
    appSetCoreState(TW_OPEN);
}

// Compute the minimum modulus allowed
uint32_t twMinimumModulusSecs()
{
    uint32_t minmod = 0;
    if (TW_LBT_PERIOD_MS != 0) {
        minmod = ((TW_LBT_PERIOD_MS*2)/1000)+1;
    }
    minmod += RADIO_TIME_WINDOW_SECS;
    return minmod;
}

// Compute the worst-case expected wait for a transmit window to occur
uint32_t appTransmitWindowWaitMaxSecs()
{
    // Modulus hasn't been received yet, use a very liberal timeout
    if (!NoteTimeValidST()) {
        return 600;
    }
    // Use modulus
    if (TWModulusSecs < twMinimumModulusSecs()) {
        TWModulusSecs = twMinimumModulusSecs();
    }
    return TWModulusSecs * 3;
}

// Compute the next transmit window and its expiration
uint32_t appNextTransmitWindowDueSecs()
{
    uint32_t now = NoteTimeST();

    // Make sure the modulus and other params are within range
    if (!NoteTimeValidST() && !MX_DBG_Active() && twSlotBeginsTime == 0) {

        // If the time is not valid, it means that we don't have a slot assigned yet.
        // When all devices awaken after a power failure, they'll all appear in slot 0
        // and collide.  This algorithm potentially steps into successive slots, but
        // it helps the startup case immensely.
        MX_RNG_Init();
        TWModulusSecs = twMinimumModulusSecs();
        twSlotBeginsTime = now + (MX_RNG_Get() % 180);
        twSlotExpiresTime = twSlotBeginsTime + 120;
        MX_RNG_DeInit();
        APP_PRINTF("%s (using random time window until assigned by gateway)\r\n", tracePeer());

    } else {

        // Keep things within legal bounds so that we can transmit
        if (TWModulusSecs < twMinimumModulusSecs()) {
            TWModulusSecs = twMinimumModulusSecs();
        }
        if (TWSlotBeginsSecs >= TWModulusSecs) {
            TWSlotBeginsSecs = 0;
        }
        if (TWSlotEndsSecs >= TWModulusSecs || TWSlotEndsSecs <= TWSlotBeginsSecs) {
            TWSlotEndsSecs = TWModulusSecs;
        }

    }

    // Compute number of seconds until the next period
    if (!NoteTimeValidST()) {

        TWSlotBeginsSecs = 0;
        TWSlotEndsSecs = TWModulusSecs;
        twSlotBeginsTime = now;
        twSlotExpiresTime = twSlotBeginsTime + TWSlotEndsSecs;
        twSlotExpiresTimeWasValid = false;

    } else {

#ifdef TW_TRACE
        APP_PRINTF("%s modulus:%d slotBegin:%d slotEnd:%d\r\n", tracePeer(), TWModulusSecs, TWSlotBeginsSecs, TWSlotEndsSecs);
#endif

        // Without an offset, all modules everywhere would be aligned to Unix epoch time 0.
        // This changes the calculations such that all modules for a given gateway are aligned
        // to a random value associated with that gateway.
        uint32_t windowRelativeNowTime = now - TWModulusOffsetSecs;

        // Compute the number of seconds until the prev and next slot, modulus those secs
        uint32_t thisWindowBeginTime = (windowRelativeNowTime / TWModulusSecs) * TWModulusSecs;
        uint32_t nextWindowBeginTime = ((windowRelativeNowTime / TWModulusSecs) + 1) * TWModulusSecs;
#ifdef TW_TRACE
        APP_PRINTF("%s relative winNow:%d winThis:%d winNext:%d\r\n",
                   tracePeer(), windowRelativeNowTime, thisWindowBeginTime, nextWindowBeginTime);
#endif

        // Adjust the slot begin based upon whether or not we've been encountering errors by
        // scheduling within the slot.  If there are multiple sensors that are misaligned because of
        // the dynamic change of modulus, this helps get them moving again by adjusting one
        // of them to get them out of sync.
        uint16_t slotBeginsSecs = TWSlotBeginsSecs;
        if (TWSlotBeginsTweak) {
            TWSlotBeginsTweak = false;
            slotBeginsSecs += (TWSlotEndsSecs - TWSlotBeginsSecs) / 2;
        }

        // If we're within the first 3 seconds of the current slot, the time is NOW
        if (windowRelativeNowTime < thisWindowBeginTime + (slotBeginsSecs + 3)) {
            twSlotBeginsTime = now + ((thisWindowBeginTime + slotBeginsSecs) - windowRelativeNowTime);
            twSlotExpiresTime = now + ((thisWindowBeginTime + TWSlotEndsSecs) - windowRelativeNowTime);
#ifdef TW_TRACE
            APP_PRINTF("%s absolute now:%d THIS slotBegin:%d slotEnd:%d\r\n",
                       tracePeer(), now, twSlotBeginsTime, twSlotExpiresTime);
#endif
        } else {
            twSlotBeginsTime = now + ((nextWindowBeginTime + slotBeginsSecs) - windowRelativeNowTime);
            twSlotExpiresTime = now + ((nextWindowBeginTime + TWSlotEndsSecs) - windowRelativeNowTime);
#ifdef TW_TRACE
            APP_PRINTF("%s absolute now:%d next slotBegin:%d slotEnd:%d\r\n",
                       tracePeer(), now, twSlotBeginsTime, twSlotExpiresTime);
#endif
        }

        if (twSlotBeginsTime < now) {
            twSlotBeginsTime = now;
        }
        twSlotExpiresTimeWasValid = false;

    }

    // If an override for a manual ping, do it now
    if (twForceIgnore) {
        twForceIgnore = false;
        return 1;
    }


    // Done
    return (twSlotBeginsTime - now);

}

// Listen if the number of LBT retries hasn't exceeded a crazy number
bool lbtListenBeforeTalk()
{

    // If retries are exhausted, give up and let caller deal with it
    if (twLBTRetriesRemaining == 0 || TWListenBeforeTalkMs == 0) {
        ListenPhaseBeforeTalk = false;
        return false;
    }
    twLBTRetriesRemaining--;

    // Listen before talk
    memset(&wireReceivedCarrier, 0, sizeof(wireReceivedCarrier));
    memset(&wireReceived, 0, sizeof(wireReceived));
    ledIndicateReceiveInProgress(true);
    ledIndicateTransmitInProgress(true);
    radioSetChannel();
    ListenPhaseBeforeTalk = true;
    if (TWListenBeforeTalkMs < TW_LBT_PERIOD_MS) {
        TWListenBeforeTalkMs = TW_LBT_PERIOD_MS;
    }
    radioRx(TWListenBeforeTalkMs);
    appSetCoreState(LOWPOWER);

    return true;

}

// Talk after a successful listen
void lbtTalk()
{
#if TRANSMIT_SIZE_TEST
    APP_PRINTF("%s send(%d)\r\n", tracePeer(), sentMessageCarrierLen);
#endif
    ledIndicateTransmitInProgress(true);
    if (!appIsGateway) {
        atpGatewayMessageSent();
    }
    radioSetChannel();
    HAL_Delay(radioWakeupRequiredMs());
    sentMessageMs = TIMER_IF_GetTimeMs();
    radioTx((uint8_t *)&sentMessageCarrier, sentMessageCarrierLen);
    appSetCoreState(LOWPOWER);
}

// Force Time Window to be ignored just for this message
void sensorIgnoreTimeWindow()
{
    twForceIgnore = true;
}

// Send a request to the gateway
void sensorSendReqToGateway(J *req, bool responseRequested)
{

    // Show the request ID
    APP_PRINTF("%s %s\r\n", tracePeer(), JGetString(req, "req"));

    // Convert it to JSON and send it
    uint8_t *reqJSON = (uint8_t *) JConvertToJSONString(req);
    uint32_t reqJSONLen = strlen((char *)reqJSON);

    // Delete the request now that it's converted
    JDelete(req);

    // Send it to the gateway
    if (reqJSON == NULL) {
        sensorSendToGateway(false, NULL, 0, false);
    } else {
        sensorSendToGateway(responseRequested, reqJSON, reqJSONLen, true);
    }

}

// Send a message to the gateway
void sensorSendToGateway(bool responseRequested, uint8_t *message, uint32_t length, bool dealloc)
{

    // Update local voltage (which may be time-consuming so we do it before I/O)
#if (SPARROW_DEVICE != BOARD_NUCLEO)
    batteryMillivolts = (uint16_t) (MX_ADC_A0_Voltage() * 1000.0);
#endif

    // Initialize retries
    sensorSendRetriesRemaining = GATEWAY_REQUEST_FAILURE_RETRIES;

    // Set sensor response state
    response.sendingRequest = true;
    response.receivingResponse = false;
    response.responseRequired = responseRequested;

    // Notify the sensor scheduler that we are sending a request
    schedSendingRequest(responseRequested);

    // Assign the next request ID, which is used to determine packet loss
    uint32_t requestID = ++LastRequestID;
    traceSetID("to", gatewayAddress, requestID);

    // Send it
    APP_PRINTF("%s sensor sending request (%d)\r\n", tracePeer(), length);
    sendToPeer(true, responseRequested ? MESSAGE_FLAG_RESPONSE : 0,
               wireReceiveRSSI, wireReceiveSNR, gatewayAddress,
               requestID, message, length, dealloc);

}

// Re-send a message to the gateway
bool sensorResendToGateway()
{

    // Exit if we've exhausted retries
    if (sensorSendRetriesRemaining == 0) {
        return false;
    }
    sensorSendRetriesRemaining--;

    // Set sensor response state
    bool responseRequested = (messageToSendFlags & MESSAGE_FLAG_RESPONSE) != 0;
    response.responseRequired = responseRequested;
    response.sendingRequest = true;
    response.receivingResponse = false;

    // Notify the sensor scheduler that we are sending a request
    schedSendingRequest(responseRequested);

    // Assign the next request ID, which is used to determine packet loss
    traceSetID("to", gatewayAddress, LastRequestID);

    // Take back possession of this buffer so it isn't freed on entry to sendToPeer()
    uint8_t *sendData = messageToSendData;
    uint32_t sendDataLen = messageToSendDataLen;
    bool sendDataDealloc = messageToSendDataDealloc;
    messageToSendData = NULL;
    messageToSendDataLen = 0;
    messageToSendDataDealloc = false;

    // Before giving up, don't use the transmit window.  This is to prevent a case in
    // which this sensor's tw is invalidly synchronized with a different sensor's tw and
    // thus they repeatedly talk over one another.
    TWSlotBeginsTweak = (sensorSendRetriesRemaining <= (GATEWAY_REQUEST_FAILURE_RETRIES/2));

    // Send it
    APP_PRINTF("%s sensor re-sending request (retries remaining: %d)\r\n", tracePeer(), sensorSendRetriesRemaining);
    sendToPeer(true, messageToSendFlags,
               wireReceiveRSSI, wireReceiveSNR, gatewayAddress,
               LastRequestID, sendData, sendDataLen, sendDataDealloc);

    // Indicate that we're retrying
    return true;

}

// See if there's a timeout on send
bool sendTimeout()
{
    if ((TIMER_IF_GetTimeMs() - sentMessageMs) > 10000) {
        return true;
    }
    return false;
}

// Process a request from a gateway
void processSensorRequest(requestState *request, bool respond)
{

    // Free the existing buffer and initialize for sending the message back
    uint8_t *reqJSON = request->data;
    uint32_t reqJSONLen = request->dataTotalLen;
    request->data = NULL;
    request->dataTotalLen = 0;
    request->dataAcknowledgedLen = 0;

    // Process the request if we haven't successfully processed it before and if no response is required
    if (!respond && request->lastProcessedRequestID != 0 && request->currentRequestID == request->lastProcessedRequestID) {
        APP_PRINTF("%s *** ignoring duplicate request ***\r\n", tracePeer());
        memset(reqJSON, '?', reqJSONLen);
        free(reqJSON);
    } else {
        uint8_t *rspData;
        uint32_t rspDataLen;
        bool success = gatewayProcessSensorRequest(request->sensorAddress, reqJSON, reqJSONLen, &rspData, &rspDataLen);
        memset(reqJSON, '?', reqJSONLen);
        free(reqJSON);
        if (success) {

            // Bump request statistics
            request->requestsProcessed++;
            if (request->lastProcessedRequestID != 0 && request->currentRequestID > request->lastProcessedRequestID) {
                request->requestsLost += (request->currentRequestID - request->lastProcessedRequestID) - 1;
            }

            // Set things up for response processing
            request->lastProcessedRequestIDForAck = request->lastProcessedRequestID;
            request->lastProcessedRequestID = request->currentRequestID;
            request->data = rspData;
            request->dataTotalLen = rspDataLen;

        }
    }

    // Transmit the response to the sensor if one was requested
    if (respond) {

        // Send response.  Note that we will retain responsibility for deallocation
        request->receivingRequest = false;
        request->sendingResponse = true;
        sendToPeer(false, 0, request->gatewayRSSI, request->gatewaySNR,
                   request->sensorAddress, request->currentRequestID,
                   request->data, request->dataTotalLen, false);

    } else {

        // Done, because no response is required
        request->receivingRequest = false;
        request->sendingResponse = false;
        if (request->data != NULL) {
            memset(request->data, '?', request->dataTotalLen);
            free(request->data);
            request->data = NULL;
        }
        gatewayWaitForAnySensorMessage();

        // Do housekeeping by borrowing time from the sensor's window
        gatewayHousekeeping(forceSensorRefresh, cachedSensors);
        forceSensorRefresh = false;

    }

}

// Wait for a message from a specific sensor
void gatewayWaitForSensorMessage()
{
    memset(&wireReceivedCarrier, 0, sizeof(wireReceivedCarrier));
    memset(&wireReceived, 0, sizeof(wireReceived));
    ledIndicateReceiveInProgress(true);
    radioSetChannel();
    wireReceiveTimeoutMs = SOLICITED_COMMS_RX_TIMEOUT_VALUE;
    ListenPhaseBeforeTalk = false;
    radioRx(wireReceiveTimeoutMs);
    APP_PRINTF("%s waiting for message from a specific sensor\r\n", tracePeer());
    appSetCoreState(LOWPOWER);
}

// Wait for a message from any sensor
void gatewayWaitForAnySensorMessage()
{
    memset(&wireReceivedCarrier, 0, sizeof(wireReceivedCarrier));
    memset(&wireReceived, 0, sizeof(wireReceived));
    ledIndicateReceiveInProgress(true);
    radioSetChannel();
    wireReceiveTimeoutMs = UNSOLICITED_RX_TIMEOUT_VALUE;
    ListenPhaseBeforeTalk = false;
    radioRx(wireReceiveTimeoutMs);
    showReceivedTime("rx", 0, 0);
    APP_PRINTF("\r\n");
    appSetCoreState(LOWPOWER);
}

// Begin a receive on gateway
void sensorWaitForGatewayMessage()
{
    memset(&wireReceivedCarrier, 0, sizeof(wireReceivedCarrier));
    memset(&wireReceived, 0, sizeof(wireReceived));
    ledIndicateReceiveInProgress(true);
    radioSetChannel();
    wireReceiveTimeoutMs = SOLICITED_COMMS_RX_TIMEOUT_VALUE;
    ListenPhaseBeforeTalk = false;
    radioRx(wireReceiveTimeoutMs);
    APP_PRINTF("%s waiting for message from gateway\r\n", tracePeer());
    appSetCoreState(LOWPOWER);
}

// Begin a receive on gateway, waiting for a response which may take a while
void sensorWaitForGatewayResponse()
{
    memset(&wireReceivedCarrier, 0, sizeof(wireReceivedCarrier));
    memset(&wireReceived, 0, sizeof(wireReceived));
    ledIndicateReceiveInProgress(true);
    radioSetChannel();
    wireReceiveTimeoutMs = SOLICITED_PROCESSING_RX_TIMEOUT_VALUE;
    ListenPhaseBeforeTalk = false;
    radioRx(wireReceiveTimeoutMs);
    APP_PRINTF("%s waiting for response from gateway\r\n", tracePeer());
    appSetCoreState(LOWPOWER);
}

// Restart the receive with the specified timeout
void restartReceive(uint32_t timeoutMs)
{
    memset(&wireReceivedCarrier, 0, sizeof(wireReceivedCarrier));
    memset(&wireReceived, 0, sizeof(wireReceived));
    ledIndicateReceiveInProgress(true);
    radioSetChannel();
    ListenPhaseBeforeTalk = false;
    radioRx(timeoutMs);
    showReceivedTime("restarting rx", 0, 0);
    APP_PRINTF("\r\n");
    appSetCoreState(LOWPOWER);
    return;
}

// Initialize sensor
void appSensorInit()
{
    // Initialize the scheduler
    schedInit();
}

// Set sensor task in a low power core state
void sensorCoreIdle()
{
    appSetCoreState(LOWPOWER);
}

// Application state machine for Sensor
void appSensorProcess()
{

#ifdef TRACE_STATE
    APP_PRINTF("ENTER %d\r\n", CurrentStateCore);
#endif

    // Default for the identity of the subject of tracing
    traceSetID("", NULL, 0);

    // Process sub-states that may have caused wakeup
    if (ButtonEventOccurred) {
        ButtonEventOccurred = false;
        appProcessButton();
        if (!appIsGateway && ledIsPairInProgress()) {
            ledSet();
            sensorPoll();
        }
    }
    if (TimerEventOccurred) {
        TimerEventOccurred = false;
        sensorPoll();
    }
    if (TraceEventOccurred) {
        TraceEventOccurred = false;
        traceInput();
    }

    // Exit if not yet paired
    if (!ledIsPairInProgress() && memcmp(gatewayAddress, invalidAddress, sizeof(gatewayAddress)) == 0) {
        APP_PRINTF("%s not currently paired with a gateway\r\n", tracePeer());
        ledReset();
        for (int i=0; i<5; i++) {
            ledWalk();
            HAL_Delay(50);
        }
        ledReset();
        sensorCoreIdle();
#ifdef TRACE_STATE
        APP_PRINTF("EXIT %d\r\n", CurrentStateCore);
#endif
        return;
    }

    // Dispatch based upon state
    switch (CurrentStateCore) {

    case TW_OPEN:
        if (NoteTimeST() >= twSlotExpiresTime) {
            APP_PRINTF("%s *** transmit window expired ***\r\n", tracePeer());
            schedRequestResponseTimeout();
            sensorCoreIdle();
            break;
        }
        twLBTRetriesRemaining = twLBTRetries;
        if (!lbtListenBeforeTalk()) {
            lbtTalk();
            break;
        }
        break;

    case RX: {

        // If we've successfully received something while we're in an LBT 'listening'
        // phase, it means that we need to try again until there is nobody speaking.
        if (ListenPhaseBeforeTalk) {
            if (lbtListenBeforeTalk()) {
                break;
            }
            APP_PRINTF("%s *** busy and transmit retries expired ***\r\n", tracePeer());
            schedRequestResponseTimeout();
            sensorCoreIdle();
            break;
        }

        // Decrypt and validate the received message, ignoring it if invalid
        if (!validateReceivedMessage()) {
            if (sendTimeout()) {
                sendMessageToPeer(false, gatewayAddress);
            } else {
                restartReceive(wireReceiveTimeoutMs);
            }
            break;
        }
        traceSetID("fm", wireReceivedCarrier.Sender, wireReceived.RequestID);

        // If this is a beacon ACK, set the gateway address and turn off beacon mode
        if (ledIsPairInProgress()) {
            if ((wireReceived.Flags & (MESSAGE_FLAG_BEACON|MESSAGE_FLAG_ACK)) == (MESSAGE_FLAG_BEACON|MESSAGE_FLAG_ACK)) {
                memcpy(gatewayAddress, wireReceivedCarrier.Sender, sizeof(gatewayAddress));
                flashConfigUpdatePeer(PEER_TYPE_SENSOR|PEER_TYPE_SELF, ourAddress, beaconKey);
#ifdef SHOW_KEYS
                APP_PRINTF("STORE OURS: ");
                for (int i=0; i<ADDRESS_LEN; i++) {
                    APP_PRINTF("%02x", ourAddress[i]);
                }
                APP_PRINTF(": ");
                for (int i=0; i<AES_KEY_BYTES; i++) {
                    APP_PRINTF("%02x", beaconKey[i]);
                }
                APP_PRINTF("\r\n");
#endif
                flashConfigUpdatePeer(PEER_TYPE_GATEWAY, gatewayAddress, invalidKey);
                memcpy(beaconKey, invalidKey, sizeof(beaconKey));
                APP_PRINTF("%s received beacon pairing ACK: paired\r\n", tracePeer());
                ledIndicatePairInProgress(false);
                ledIndicateAck(2);
                // This restart isn't strictly necessary, but it's good
                // to get everything into a known state with a fresh start.
#ifdef KEEP_ALIVE_WHEN_PAIRED
                sensorCoreIdle();
#else
                NVIC_SystemReset();
#endif
                break;
            }
        }

        // Error if the sender is not the gateway we're paired with
        if (memcmp(gatewayAddress, wireReceivedCarrier.Sender, sizeof(gatewayAddress)) != 0) {
            APP_PRINTF("%s message received by sensor from wrong gateway\r\n", tracePeer());
            if (sendTimeout()) {
                sendMessageToPeer(false, gatewayAddress);
            } else {
                restartReceive(wireReceiveTimeoutMs);
            }
            break;
        }

        // We're sending a request to the gateway and we get an ack on a chunk
        if ((wireReceived.Flags & MESSAGE_FLAG_ACK) != 0) {
            APP_PRINTF("%s ack received\r\n", tracePeer());

            // Extract and set the sensor time
            if (wireReceived.Len >= sizeof(gatewayAckBody)-SENSOR_NAME_MAX) {
                gatewayAckBody *body = (gatewayAckBody *)wireReceived.Body;

                // Extract sensor name
                uint32_t sensorNameLen = wireReceived.Len - (sizeof(gatewayAckBody)-SENSOR_NAME_MAX);
                if (sensorNameLen == 0) {
                    sensorName[0] = '\0';
                } else {
                    memcpy(sensorName, body->Name, sizeof(sensorName));
                    sensorName[sensorNameLen-1] = '\0';
                    for (size_t i=0; i<sensorNameLen-1; i++) {
                        if (sensorName[i] < ' ') {
                            sensorName[i] = '?';
                        }
                    }
                }

                // If the gateway's boot time has changed, then restart just as a
                // way of resetting the world remotely.
                if (body->BootTime != 0) {
                    if (gatewayBootTime != 0 && body->BootTime != gatewayBootTime) {
                        NVIC_SystemReset();
                    }
                    gatewayBootTime = body->BootTime;
                }

                // Extract the gateway's date/time and set it locally
                char zone[4];
                zone[0] = body->ZoneName[0];
                zone[1] = body->ZoneName[1];
                zone[2] = body->ZoneName[2];
                zone[3] = '\0';
                NoteTimeSet(body->Time, body->ZoneOffsetMins, zone, NULL, NULL);

                // Trace
                if (TWModulusSecs != body->TWModulusSecs) {
                    APP_PRINTF("%s TWModulusSecs: from %d to %d\r\n", tracePeer(), TWModulusSecs, body->TWModulusSecs);
                }
                if (TWModulusOffsetSecs != body->TWModulusOffsetSecs) {
                    APP_PRINTF("%s TWModulusOffsetSecs: from %d to %d\r\n", tracePeer(), TWModulusOffsetSecs, body->TWModulusOffsetSecs);
                }
                if (TWSlotBeginsSecs != body->TWSlotBeginsSecs) {
                    APP_PRINTF("%s TWSlotBeginsSecs: from %d to %d\r\n", tracePeer(), TWSlotBeginsSecs, body->TWSlotBeginsSecs);
                }
                if (TWSlotEndsSecs != body->TWSlotEndsSecs) {
                    APP_PRINTF("%s TWSlotEndsSecs: from %d to %d\r\n", tracePeer(), TWSlotEndsSecs, body->TWSlotEndsSecs);
                }

                // Set the time window parameters
                TWModulusSecs = body->TWModulusSecs;
                TWModulusOffsetSecs = body->TWModulusOffsetSecs;
                TWSlotBeginsSecs = body->TWSlotBeginsSecs;
                TWSlotEndsSecs = body->TWSlotEndsSecs;
                TWListenBeforeTalkMs = body->TWListenBeforeTalkMs;

                // Adapt the transmit power parameters based what gateway sees
                if (wireReceived.RSSI != 0 || wireReceived.SNR != 0) {
                    atpGatewayMessageReceived(wireReceived.RSSI, wireReceived.SNR,  // the gateway's view of our signal
                                              wireReceiveRSSI, wireReceiveSNR);     // our view of the gateway's signal
                }

            }

            // Send the next chunk of the request
            messageToSendAcknowledgedLen += sentMessage.Len;
            if (messageToSendAcknowledgedLen < messageToSendDataLen) {
                sendMessageToPeer(false, gatewayAddress);
                break;
            }

            // If a response is coming, wait for that response from the gateway
            schedRequestCompleted();
            response.sendingRequest = false;
            response.receivingResponse = false;
            if (response.responseRequired) {
                sensorWaitForGatewayResponse();
            } else {
                APP_PRINTF("%s received final ACK: request completed\r\n", tracePeer());
                sensorCoreIdle();
            }
            break;

        }

        // If this is the first chunk of the response, allocate the receive buffer.  Note that
        // we are careful to allocate 1 byte more than TotalLen because after the response is
        // received we will need to convert it to a null-terminated string so we can parse it.
        if (wireReceived.Offset == 0 || wireReceived.RequestID != response.requestID) {
            if (response.data != NULL) {
                memset(response.data, '?', response.dataTotalLen);
                free(response.data);
                response.data = NULL;
            }
            APP_PRINTF("%s now receiving response from gateway\r\n", tracePeer());
            response.receivingResponse = true;
            response.sendingRequest = false;
            response.data = (uint8_t *) malloc(wireReceived.TotalLen+1);
            response.dataTotalLen = wireReceived.TotalLen;
            response.dataAcknowledgedLen = 0;
            response.requestID = wireReceived.RequestID;
        }

        // If this is a duplicate, skip it
        if (wireReceived.Offset+wireReceived.Len == response.dataAcknowledgedLen) {

            APP_PRINTF("%s *** re-acking duplicate message ***\r\n", tracePeer());

        } else {

            // If we're not synchronized on where within the response the transfer is, error
            if (wireReceived.Offset != response.dataAcknowledgedLen) {
                APP_PRINTF("%s *** message has wrong offset *** (%d/%d)\r\n",
                           tracePeer(), wireReceived.Offset, response.dataAcknowledgedLen);
                schedRequestResponseTimeout();
                sensorCoreIdle();
                break;
            }
            if (wireReceived.Offset+wireReceived.Len > response.dataTotalLen) {
                APP_PRINTF("%s *** message has wrong length ***\r\n", tracePeer());
                schedRequestResponseTimeout();
                sensorCoreIdle();
                break;
            }

            // Append the successfully received data to the response buffer
            if (response.data != NULL && wireReceived.Len > 0) {
                memcpy(&response.data[response.dataAcknowledgedLen], wireReceived.Body, wireReceived.Len);
                response.dataAcknowledgedLen += wireReceived.Len;
            }

        }

        // We can no longer retry this request because we're about to free the message buffer
        sensorSendRetriesRemaining = 0;

        // Ack this received packet
        messageToSendRequestID = response.requestID;
        messageToSendFlags = MESSAGE_FLAG_ACK;
        freeMessageToSendBuffer();
        sendMessageToPeer(false, gatewayAddress);
        break;
    }

    case TX: {

        // Process the gateway response when it's completely received
        traceSetID("to", sentMessageCarrier.Receiver, sentMessage.RequestID);
        if (response.receivingResponse && response.dataAcknowledgedLen == response.dataTotalLen) {
            response.sendingRequest = false;
            response.receivingResponse = false;

            // Convert it to a null-terminated string and parse it.  Note that we had explicitly
            // allocated this buffer 1 byte larger than we had needed explicitly for this purpose.
            response.data[response.dataTotalLen] = '\0';
            J *rsp = JConvertFromJSONString((const char *)response.data);
            if (rsp == NULL) {
                APP_PRINTF("%s *** sensor response isn't valid JSON *** (%d)\r\n", tracePeer(), response.dataTotalLen);
            } else {
                schedResponseCompleted(rsp);
                JDelete(rsp);
                if (twSlotExpiresTimeWasValid && NoteTimeValidST()) {
                    uint32_t now = NoteTimeST();
                    if (now > twSlotExpiresTime) {
                        APP_PRINTF("%s *** sensor used too much time (%d)\r\n", tracePeer(), now-twSlotExpiresTime);
                    } else {
                        APP_PRINTF("%s sensor completed with time to spare (%d)\r\n", tracePeer(), twSlotExpiresTime-now);
                    }
                }
            }

            // Go idle
            sensorCoreIdle();
            break;
        }

        // We're done when our request is fully acknowledged
        if (response.sendingRequest && response.dataAcknowledgedLen == response.dataTotalLen) {
            response.sendingRequest = false;
            response.receivingResponse = false;
        }

        // Wait for the next chunk from the gateway
        sensorWaitForGatewayMessage();
        break;
    }

    case RX_TIMEOUT:

        // If in LBT mode, this means the channel is clear and we can now transmit freely
        if (ListenPhaseBeforeTalk) {
            lbtTalk();
            break;
        }

        // We expected either an ACK or response data and failed to receive it
        sensorGatewayRequestFailure(false, "*** no gateway response ***");
        break;

    case RX_ERROR:

        // If in LBT mode, this means the channel is busy and we couldn't successfully receive,
        // so we should either retry the listen or give up.
        if (ListenPhaseBeforeTalk) {
            if (lbtListenBeforeTalk()) {
                break;
            }
            sensorGatewayRequestFailure(false, "*** can't transmit because channel is busy ***");
            break;
        }

        // Abandon hope of receiving the ACk or response data
        sensorGatewayRequestFailure(false, "*** error receiving from gateway ***");
        break;

    case TX_TIMEOUT:
        sensorGatewayRequestFailure(true, "*** can't transmit to gateway ***");
        break;

    case LOWPOWER:
    default:
        break;
    }

#ifdef TRACE_STATE
    APP_PRINTF("EXIT %d\r\n", CurrentStateCore);
#endif

}

// Handle the case of known failure of transmit or receive to a gateway having failed
void sensorGatewayRequestFailure(bool wasTX, const char *why)
{

    // Indicate failure
    if (wasTX) {
        traceSetID("to", gatewayAddress, LastRequestID);
    } else {
        traceSetID("fm", gatewayAddress, LastRequestID);
    }
    APP_PRINTF("%s %s\r\n", tracePeer(), why);

    // Indicate to the ATP subsystem that we lost a message
    // 2021-04-19 move to this side of ResendToGateway so that we take every failure into account
    // so that we are much more accurately responsive to losses, as opposed to suppressing the
    // failures at the "boundary" that might happen as often as every other message.
    atpGatewayMessageLost();

    // If we can retry, do so during the next transmit window
    if (sensorResendToGateway()) {
        return;
    }

    // Free the message buffer
    freeMessageToSendBuffer();

    // Abort with a lost message indication
    memset(&wireReceivedCarrier, 0, sizeof(wireReceivedCarrier));
    memset(&wireReceived, 0, sizeof(wireReceived));
    schedRequestResponseTimeout();
    sensorCoreIdle();

}

// Initialize gateway state machine
void appGatewayInit()
{
    gatewayHousekeeping(false, cachedSensors);
    gatewayWaitForAnySensorMessage();
}

// Application state machine for Gateway
void appGatewayProcess()
{

#ifdef TRACE_STATE
    APP_PRINTF("ENTER %d\r\n", CurrentStateCore);
#endif

    // Set identity of the 'subject' of our work to 'unknown'
    traceSetID("", ourAddress, 0);

    // Process sub-states that may have caused wakeup
    if (ButtonEventOccurred) {
        ButtonEventOccurred = false;
        appProcessButton();
    }
    if (TraceEventOccurred) {
        TraceEventOccurred = false;
        traceInput();
        gatewayHousekeeping(forceSensorRefresh, cachedSensors);
    }

    // Dispatch based upon state
    switch (CurrentStateCore) {

    case TW_OPEN:
        APP_PRINTF("%s *** INVALID STATE FOR GATEWAY ***\r\n", tracePeer());
        gatewayWaitForAnySensorMessage();
        break;

    case RX: {

        // If we've successfully received something while we're in an LBT 'listening'
        // phase, it means that we need to try again until there is nobody speaking.
        if (ListenPhaseBeforeTalk) {
            if (lbtListenBeforeTalk()) {
                break;
            }
            APP_PRINTF("%s *** busy and transmit retries expired ***\r\n", tracePeer());
            gatewayWaitForAnySensorMessage();
            break;
        }

        // Decrypt and validate the received message, ignoring it if invalid
        if (!validateReceivedMessage()) {
            restartReceive(wireReceiveTimeoutMs);
            break;
        }
        traceSetID("fm", wireReceivedCarrier.Sender, wireReceived.RequestID);

        // If this request is from a different sensor than last time, clear stats
        if (memcmp(lastReceivedSensorAddress, wireReceivedCarrier.Sender, sizeof(wireReceivedCarrier.Sender)) != 0) {
            memcpy(lastReceivedSensorAddress, wireReceivedCarrier.Sender, sizeof(lastReceivedSensorAddress));
            radioSetTxPowerUnknown();
        }

        // Find the sensor that is sending to us, and bubble it down to the 0th entry of the cache
        requestState foundRequest;
        int found = -1;
        for (int i=0; i<cachedSensors; i++) {
            if (memcmp(requestCache[i].sensorAddress, wireReceivedCarrier.Sender, sizeof(wireReceivedCarrier.Sender)) == 0) {
                foundRequest = requestCache[i];
                found = i;
                break;
            }
        }
        if (found == -1) {
            if (cachedSensors < MAX_CACHED_SENSORS) {
                cachedSensors++;
            }
            memset(&foundRequest, 0, sizeof(requestState));
            memcpy(foundRequest.sensorAddress, wireReceivedCarrier.Sender, sizeof(wireReceivedCarrier.Sender));
            APP_PRINTF("%s *** new sensor being cached ***\r\n", tracePeer());
            found = cachedSensors-1;
            forceSensorRefresh = true;
        }
        for (int i=cachedSensors-1; i>0; --i) {
            if (i <= found) {
                requestCache[i] = requestCache[i-1];
            }
        }
        requestCache[0] = foundRequest;
        requestState *request = &requestCache[0];
        request->lastReceivedTime = NoteTimeST();
        traceSetID("fm", request->sensorAddress, request->currentRequestID);
        APP_PRINTF("%s rcv txp:%d rssi:%d snr:%d\r\n", tracePeer(), wireReceived.TXP, wireReceived.RSSI, wireReceived.SNR);

        // Remember the radio stats
        if (wireReceiveSignalValid && (wireReceiveRSSI != 0 || wireReceiveSNR != 0)) {
            request->gatewayRSSI = wireReceiveRSSI;
            request->gatewaySNR = wireReceiveSNR;
        }
        request->sensorRSSI = wireReceived.RSSI;
        request->sensorSNR = wireReceived.SNR;
        request->sensorTXP = wireReceived.TXP;
        request->sensorLTP = wireReceived.LTP;
        request->sensorMv = wireReceived.Millivolts;

        // Notify atp subsystem of the power of the last message received
        atpMatchPowerLevel(wireReceived.TXP);

        // Display time of receipt
        showReceivedTime((wireReceived.Flags & MESSAGE_FLAG_ACK) != 0 ? "rcv ack" : "rcv msg",
                         request->twSlotBeginsSecs, request->twSlotEndsSecs);

        // We're sending a response back to the sensor and we get an ack on a chunk
        if ((wireReceived.Flags & MESSAGE_FLAG_ACK) != 0) {

            // Send the next chunk of the response
            if (request->dataAcknowledgedLen < request->dataTotalLen) {
                freeMessageToSendBuffer();
                messageToSendData = request->data;
                messageToSendDataLen = request->dataTotalLen;
                messageToSendAcknowledgedLen = request->dataAcknowledgedLen;
                messageToSendRequestID = request->currentRequestID;
                messageToSendFlags = 0;
                sendMessageToPeer(false, request->sensorAddress);
                break;
            }

            // Done sending the final response chunk
            if (request->data != NULL) {
                memset(request->data, '?', request->dataTotalLen);
                free(request->data);
                request->data = NULL;
            }

            // Wait for next incoming message from anyone
            request->receivingRequest = false;
            request->sendingResponse = false;
            gatewayWaitForAnySensorMessage();
            break;

        }

        // If this is the first chunk of the message, allocate the receive buffer
        if (wireReceived.Offset == 0 || wireReceived.RequestID != request->currentRequestID) {
            if (request->data != NULL) {
                memset(request->data, '?', request->dataTotalLen);
                free(request->data);
                request->data = NULL;
            }
            request->receivingRequest = true;
            request->sendingResponse = false;
            request->responseRequired = (wireReceived.Flags & MESSAGE_FLAG_RESPONSE) != 0;
            request->data = (uint8_t *) malloc(wireReceived.TotalLen);
            request->dataTotalLen = wireReceived.TotalLen;
            request->dataAcknowledgedLen = 0;
            request->currentRequestID = wireReceived.RequestID;
            traceSetID("fm", request->sensorAddress, request->currentRequestID);
            APP_PRINTF("%s now receiving request from sensor\r\n", tracePeer());
        }

        // If this is a duplicate, skip it
        if (wireReceived.Offset+wireReceived.Len == request->dataAcknowledgedLen) {

            APP_PRINTF("%s *** re-acking duplicate message ***\r\n", tracePeer());

        } else {

            // If we're not synchronized on where within the request the transfer is, error
            if (wireReceived.Offset != request->dataAcknowledgedLen) {
                APP_PRINTF("%s *** message has wrong offset *** (%d/%d)\r\n", tracePeer(),
                           wireReceived.Offset, request->dataAcknowledgedLen);
                gatewayWaitForAnySensorMessage();
                break;
            }
            if (wireReceived.Offset+wireReceived.Len > request->dataTotalLen) {
                request->receivingRequest = true;
                request->sendingResponse = false;
                APP_PRINTF("%s *** message has wrong length ***\r\n", tracePeer());
                gatewayWaitForAnySensorMessage();
                break;
            }

            // Append the successfully received data to the request buffer
            if (request->data != NULL && wireReceived.Len > 0) {
                memcpy(&request->data[request->dataAcknowledgedLen], wireReceived.Body, wireReceived.Len);
                request->dataAcknowledgedLen += wireReceived.Len;
            }

        }

        // If this was a beacon, we're completing a peering request
        if ((wireReceived.Flags & MESSAGE_FLAG_BEACON) != 0) {

            // Verify that it's a supported algorithm
            if (wireReceived.RequestID != MESSAGE_ALG_CTR) {
                APP_PRINTF("%s *** beacon message has wrong encryption type ***\r\n", tracePeer());
                gatewayWaitForAnySensorMessage();
                break;
            }

            // Update the key and algorithm for this peer in flash
            flashConfigUpdatePeer(PEER_TYPE_SENSOR, request->sensorAddress, wireReceived.Body);
            APP_PRINTF("%s *** beacon: updated sensor key\r\n", tracePeer());
#ifdef SHOW_KEYS
            APP_PRINTF("STORE PEER: ");
            for (int i=0; i<ADDRESS_LEN; i++) {
                APP_PRINTF("%02x", request->sensorAddress[i]);
            }
            APP_PRINTF(": ");
            for (int i=0; i<AES_KEY_BYTES; i++) {
                APP_PRINTF("%02x", wireReceived.Body[i]);
            }
            APP_PRINTF("\r\n");
#endif

            // Notify the notecard and notehub that we've peered with this sensor
            J *req = NoteNewCommand("hub.log");
            if (req != NULL) {
                JAddStringToObject(req, "method", "sensor-provision");
                char saddr[40];
                utilAddressToText(request->sensorAddress, saddr, sizeof(saddr));
                JAddStringToObject(req, "text", saddr);
                JAddBoolToObject(req, "sync", true);
                NoteRequest(req);
            }

        }

        // Prepare the body
        static gatewayAckBody body = {0};
        char *zone;
        int offset;
        char name[SENSOR_NAME_MAX];
        flashConfigFindPeerByAddress(request->sensorAddress, NULL, NULL, name);
        extractNameComponents(name, body.Name, NULL, 0);
        body.LastProcessedRequestID = request->lastProcessedRequestIDForAck;
        twRefresh();
        body.TWModulusSecs = TWModulusSecs;
        body.TWModulusOffsetSecs = TWModulusOffsetSecs;
        body.TWSlotBeginsSecs = request->twSlotBeginsSecs;
        body.TWSlotEndsSecs = request->twSlotEndsSecs;
        body.TWListenBeforeTalkMs = TWListenBeforeTalkMs;
#if REBOOT_SENSORS_WHEN_GATEWAY_REBOOTS
        body.BootTime = gatewayBootTime;
#else
        body.BootTime = 0;
#endif
        NoteRegion(NULL, NULL, &zone, &offset);
        body.ZoneOffsetMins = offset;
        body.ZoneName[0] = zone[0];
        body.ZoneName[1] = zone[1];
        body.ZoneName[2] = zone[2];

        // Set the time to be our current time plus an offset of the transmit window,
        // so that we are as close as possible to synchronized times.  There is also
        // unfortunately a "transit time" for is message to be AES-encrypted and
        // transmitted over the wire (given the slow LoRa wire speed), so an
        // additional adjustment is made.  (This was measured empically.)
        body.Time = NoteTimeST();
        body.Time += 4;                 // Estimated transit time from gateway to sensor
        if (TW_LBT_PERIOD_MS != 0) {    // Delay in transmitting when window is opened
            body.Time += (TW_LBT_PERIOD_MS/1000)+1;
        }

        // Make sure that the send buffer is deallocated
        freeMessageToSendBuffer();

        // Set the length to what's necessary to transmit the name, using the
        // assumption that the name is always at the very end of the structure.
        messageToSendDataLen = sizeof(body);
        messageToSendDataLen -= SENSOR_NAME_MAX;
        messageToSendDataLen += strlen(body.Name)+1;

        // Ack this received packet with the current gateway time
        messageToSendRequestID = request->currentRequestID;
        messageToSendFlags = MESSAGE_FLAG_ACK;
        if ((wireReceived.Flags & MESSAGE_FLAG_BEACON) != 0) {
            messageToSendFlags |= MESSAGE_FLAG_BEACON;
        }
        messageToSendData = (uint8_t *) &body;
        messageToSendAcknowledgedLen = 0;
        sendMessageToPeer(false, request->sensorAddress);
        break;
    }

    case TX: {

        // If we just sent a beacon response, we're done.
        if ((messageToSendFlags & MESSAGE_FLAG_BEACON) != 0) {
            gatewayWaitForSensorMessage();
            break;
        }

        // The last received message is the one most recent in the cache
        requestState *request = &requestCache[0];
        traceSetID("to", request->sensorAddress, request->currentRequestID);
        if (memcmp(sentMessageCarrier.Receiver, request->sensorAddress, sizeof(request->sensorAddress)) != 0) {
            APP_PRINTF("%s $$$ WRONG SENDER $$$\r\n", tracePeer());
        }

        // Process the sensor request when it's completely received
        if (request->receivingRequest) {
            if (request->dataAcknowledgedLen == request->dataTotalLen) {
                request->receivingRequest = false;
                request->sendingResponse = false;
                processSensorRequest(request, request->responseRequired);
                break;
            }
        }

        // We're done when our response is fully acknowledged
        if (request->sendingResponse) {
            request->dataAcknowledgedLen += sentMessage.Len;
            if (request->dataAcknowledgedLen >= sentMessage.TotalLen) {
                request->receivingRequest = false;
                request->sendingResponse = false;
                gatewayWaitForSensorMessage();
                break;
            }
        }

        // Wait for the next chunk from the sensor
        gatewayWaitForSensorMessage();
        break;
    }

    // Expected wait
    case RX_TIMEOUT:
        if (ListenPhaseBeforeTalk) {
            lbtTalk();
            break;
        }
        traceSetID("fm", wireReceivedCarrier.Sender, wireReceivedCarrier.Message.RequestID);
        if (wireReceiveTimeoutMs != UNSOLICITED_RX_TIMEOUT_VALUE) {
            APP_PRINTF("%s *** no response from sensor ***\r\n", tracePeer());
        }
        gatewayWaitForAnySensorMessage();
        break;

    case RX_ERROR:
        memset(&wireReceivedCarrier, 0, sizeof(wireReceivedCarrier));
        memset(&wireReceived, 0, sizeof(wireReceived));
        traceSetID("fm", wireReceivedCarrier.Sender, wireReceivedCarrier.Message.RequestID);

        // If in LBT mode, this means the channel is busy and we couldn't successfully receive,
        // so we should either retry the listen or give up.
        if (ListenPhaseBeforeTalk) {
            if (lbtListenBeforeTalk()) {
                break;
            }
            showReceivedTime("*** rx error and transmit retries expired ***", 0, 0);
            sensorCoreIdle();
            break;
        }
        showReceivedTime("*** error receiving from sensor ***", 0, 0);
        restartReceive(wireReceiveTimeoutMs);
        break;

    case TX_TIMEOUT:
        traceSetID("to", sentMessageCarrier.Receiver, sentMessage.RequestID);
        APP_PRINTF("%s *** can't transmit to sensor ***\r\n", tracePeer());
        gatewayWaitForAnySensorMessage();
        break;

    case LOWPOWER:
    default:
        break;
    }

#ifdef TRACE_STATE
    APP_PRINTF("EXIT %d\r\n", CurrentStateCore);
#endif

}

// Show the time that a message was received, as well as when it SHOULD have been received
void showReceivedTime(char *msg, uint32_t beginSecs, uint32_t endSecs)
{
    uint32_t now = NoteTimeST();
    uint32_t modSecs = (TWModulusSecs == 0 ? twMinimumModulusSecs() : TWModulusSecs);
    uint32_t slotSecs = twMinimumModulusSecs();
    uint32_t windowRelativeNowTime = now - TWModulusOffsetSecs;
    uint32_t thisWindowBeginTime = (windowRelativeNowTime / modSecs) * modSecs;
    uint32_t nextWindowBeginTime = ((windowRelativeNowTime / modSecs) + 1) * modSecs;
    uint32_t thisSlotNumber = (windowRelativeNowTime - thisWindowBeginTime) / slotSecs;
    uint32_t thisSlotOffset = (windowRelativeNowTime - thisWindowBeginTime) % slotSecs;
    uint32_t thisSlotBeginTime = (thisSlotNumber * slotSecs) + thisWindowBeginTime;
    uint32_t nextSlotBeginTime = thisSlotBeginTime + slotSecs;
    uint32_t sensorSlotNumber = beginSecs / slotSecs;
    uint32_t sensorSlotBeginTime = thisWindowBeginTime + beginSecs;
    uint32_t sensorSlotEndTime = thisWindowBeginTime + endSecs;
    DEBUG_VARIABLE(nextWindowBeginTime);
    DEBUG_VARIABLE(thisSlotOffset);
    DEBUG_VARIABLE(nextSlotBeginTime);
    DEBUG_VARIABLE(sensorSlotEndTime);
    APP_PRINTF("%s %s", tracePeer(), msg);
    if (appIsGateway && NoteTimeValidST() && thisWindowBeginTime > gatewayBootTime) {
        APP_PRINTF(" window:%d-%d", thisWindowBeginTime-gatewayBootTime, nextWindowBeginTime-gatewayBootTime);
        APP_PRINTF(" slot#%d/%d(", thisSlotNumber, modSecs/slotSecs);
        char slotOwner[SENSOR_NAME_MAX];
        strlcpy(slotOwner, "+++ UNKNOWN +++", sizeof(slotOwner));
        for (int i=0; i<cachedSensors; i++) {
            if ((thisSlotBeginTime-thisWindowBeginTime) == requestCache[i].twSlotBeginsSecs) {
                flashConfigFindPeerByAddress(requestCache[i].sensorAddress, NULL, NULL, slotOwner);
                break;
            }
        }
        APP_PRINTF("%s)=%d%%%d:%d-%d", slotOwner, thisSlotOffset, slotSecs,
                   thisSlotBeginTime-thisWindowBeginTime, nextSlotBeginTime-thisWindowBeginTime);
        if (endSecs > 0) {
            APP_PRINTF("sensor#%d(", sensorSlotNumber);
            strlcpy(slotOwner, "+++ UNKNOWN +++", sizeof(slotOwner));
            for (int i=0; i<cachedSensors; i++) {
                if (beginSecs == requestCache[i].twSlotBeginsSecs) {
                    flashConfigFindPeerByAddress(requestCache[i].sensorAddress, NULL, NULL, slotOwner);
                    break;
                }
            }
            APP_PRINTF("%s)=%d:%d-%d", slotOwner, endSecs-beginSecs,
                       sensorSlotBeginTime-thisWindowBeginTime, sensorSlotEndTime-thisWindowBeginTime);
            if (thisSlotBeginTime != sensorSlotBeginTime || thisSlotNumber != sensorSlotNumber) {
                APP_PRINTF(" +++ WRONG SLOT +++");
            }
        }
    }
    APP_PRINTF("\r\n");
    HAL_Delay(5);   // Flush output before sleep
}

// Validate the received message, making sure that it's for us, and setting wireReceiveMessageError
bool validateReceivedMessage()
{

    // Clear the message because it's not yet decrypted
    traceSetID("fm", 0, 0);

    // Exit if not the right protocol version
    if (wireReceivedCarrier.Version != MESSAGE_VERSION) {
        APP_PRINTF("%s invalid protocol version\r\n", tracePeer());
        return false;
    }

    // Exit if not intended for us
    if (appIsGateway && ledIsPairInProgress() && memcmp(wildcardAddress, wireReceivedCarrier.Receiver, sizeof(ourAddress)) == 0) {
        APP_PRINTF("%s received pairing beacon\r\n", tracePeer());
    }  else {
        if (memcmp(ourAddress, wireReceivedCarrier.Receiver, sizeof(ourAddress)) != 0) {
            APP_PRINTF("%s message not intended for us\r\n", tracePeer());
            return false;
        }
    }

    // If it's cleartext, we're done
    if (wireReceivedCarrier.Algorithm == MESSAGE_ALG_CLEAR) {
        if (wireReceivedCarrier.MessageLen > sizeof(wireReceived)) {
            APP_PRINTF("%s cleartext message has incorrect length\r\n", tracePeer());
            return false;
        }
        memcpy((uint8_t *)&wireReceived, (uint8_t *)&wireReceivedCarrier.Message, wireReceivedCarrier.MessageLen);
        return true;
    }

    // Exit if not a supported crypto version
    if (wireReceivedCarrier.Algorithm != MESSAGE_ALG_CTR) {
        APP_PRINTF("%s unsupported encryption type\r\n", tracePeer());
        return false;
    }

    // Always use the sensor's key when decrypting
    uint8_t key[AES_KEY_BYTES];
    uint8_t *sensorAddress = appIsGateway ? wireReceivedCarrier.Sender : wireReceivedCarrier.Receiver;
    if (!flashConfigFindPeerByAddress(sensorAddress, NULL, key, NULL)) {
        APP_PRINTF("%s can't find the sensor's key\r\n", tracePeer());
        return false;
    }

    // Trace
#ifdef SHOW_KEYS
    APP_PRINTF("DECRYPT: ");
    for (int i=0; i<ADDRESS_LEN; i++) {
        APP_PRINTF("%02x", sensorAddress[i]);
    }
    APP_PRINTF(": ");
    for (int i=0; i<sizeof(key); i++) {
        APP_PRINTF("%02x", key[i]);
    }
    APP_PRINTF("\r\n");
#endif

    // Decrypt it
    bool success = MX_AES_CTR_Decrypt(key, (uint8_t *)&wireReceivedCarrier.Message, wireReceivedCarrier.MessageLen, (uint8_t *)&wireReceived);
    memcpy(key, invalidKey, sizeof(key));
    if (success && wireReceived.Signature != MESSAGE_SIGNATURE) {
        success = false;
    }

    // Fail if can't decrypt
    if (!success) {
        APP_PRINTF("%s can't decrypt received message\r\n", tracePeer());
        return false;
    }

    // Successful message reception
    return true;

}

// Use the request cache to re-compute the time window parameters, based on the
// devices that we consider "active"
void twRefresh()
{

    // Count the active sensors
    uint32_t inactiveTime = NoteTimeST() - TW_ACTIVE_SECS;
    uint32_t activeSensors = 0;
    for (int i=0; i<cachedSensors; i++) {
        if (memcmp(requestCache[i].sensorAddress, gatewayAddress, ADDRESS_LEN) == 0) {
            continue;
        }
        if (requestCache[i].lastReceivedTime < inactiveTime) {
            continue;
        }
        activeSensors++;
    }

    // Only reassign slots if we change active sensors
    if (twLastActiveSensors == activeSensors) {
        return;
    }
    twLastActiveSensors = activeSensors;

    // Force the database to be updated
    APP_PRINTF("%s **** active sensors changed to %d ****\r\n", tracePeer(), activeSensors);
    forceSensorRefresh = true;

    // Update active sensors and modulus, assigning a modulus offset to keep us from
    // interfering with other local gateways
    TWModulusSecs = activeSensors * twMinimumModulusSecs();
    MX_RNG_Init();
    TWModulusOffsetSecs = MX_RNG_Get() % 123;
    MX_RNG_DeInit();
    TWListenBeforeTalkMs = TW_LBT_PERIOD_MS;

    // Re-assign slots to active sensors
    uint32_t slotBeginsSecs = 0;
    for (int i=0; i<cachedSensors; i++) {
        requestCache[i].twSlotBeginsSecs = 0;
        if (memcmp(requestCache[i].sensorAddress, gatewayAddress, ADDRESS_LEN) == 0) {
            continue;
        }
        if (requestCache[i].lastReceivedTime < inactiveTime) {
            continue;
        }

        // Update the slot
        requestCache[i].twSlotBeginsSecs = slotBeginsSecs;
        requestCache[i].twSlotEndsSecs = slotBeginsSecs + twMinimumModulusSecs();
        slotBeginsSecs += twMinimumModulusSecs();

        // Display the slot assignment
        char msg[40];
        utilAddressToText(requestCache[i].sensorAddress, msg, sizeof(msg));
        APP_PRINTF("%s %s assigned slot %d-%d\r\n", tracePeer(), msg, requestCache[i].twSlotBeginsSecs, requestCache[i].twSlotEndsSecs);

    }

}

// Clear request info in a cache entry
void appSensorCacheEntryResetStats(uint32_t index)
{
    requestCache[index].requestsProcessed = 0;
    requestCache[index].requestsLost = 0;
}

// Get info about a sensor cache entry
bool appSensorCacheEntry(uint32_t i, uint8_t *address,
                         int8_t *gatewayRSSI, int8_t *gatewaySNR,
                         int8_t *sensorRSSI, int8_t *sensorSNR,
                         int8_t *sensorTXP, int8_t *sensorLTP, uint16_t *sensorMv,
                         uint32_t *lastReceivedTime,
                         uint32_t *requestsProcessed, uint32_t *requestsLost)
{

    // Out of range
    if (i >= cachedSensors) {
        return false;
    }

    // Skim off entries that should be ignored
    if (memcmp(requestCache[i].sensorAddress, gatewayAddress, ADDRESS_LEN) == 0) {
        return false;
    }

    // Return the info
    if (address != NULL) {
        memcpy(address, requestCache[i].sensorAddress, ADDRESS_LEN);
    }
    *gatewayRSSI = requestCache[i].gatewayRSSI;
    *gatewaySNR = requestCache[i].gatewaySNR;
    *sensorRSSI = requestCache[i].sensorRSSI;
    *sensorSNR = requestCache[i].sensorSNR;
    *sensorTXP = requestCache[i].sensorTXP;
    *sensorLTP = requestCache[i].sensorLTP;
    *sensorMv = requestCache[i].sensorMv;
    *lastReceivedTime = requestCache[i].lastReceivedTime;
    *requestsProcessed = requestCache[i].requestsProcessed;
    *requestsLost = requestCache[i].requestsLost;

    // Done
    return true;

}

// See if we are being asked to do something related to a button
bool appProcessButton()
{

    switch (ledButtonCheck()) {

    case BUTTON_HELD:
        flashConfigFactoryReset();
        return true;

    case BUTTON_PRESSED:
        // Single tap on gateway always toggles pairing
        if (ledIsPairInProgress() && !ledIsPairMandatory()) {
            ledIndicatePairInProgress(false);
        } else {
            if (appIsGateway) {
                ledIndicatePairInProgress(true);
            } else {
#if TRANSMIT_SIZE_TEST
                appSendLoRaPacketSizeTestPing();
#else
                // If we're not yet paired, a single button
                // press will initiate pairing.  Otherwise,
                // send the button-press to a sensor app.
                if (ledIsPairMandatory()) {
                    ledIndicatePairInProgress(true);
                } else {
                    schedDispatchISR(BUTTON1_Pin);
                }
#endif
            }
        }
        return true;

    case BUTTON_HOLD_ABORTED:
        if (ledIsPairInProgress() && !ledIsPairMandatory()) {
            ledIndicatePairInProgress(false);
        } else {
            if (appIsGateway) {
                appEnterSoftAP();
            } else {
#ifdef LONG_PRESS_TO_ENTER_PAIRING_MODE
                ledIndicatePairInProgress(true);
                if (!appIsGateway) {
                    sensorPoll();
                }
#endif
            }
        }
        return true;

    }

    return false;

}

// Perform a LoRa transmit test when trying to determine
// maximum packet size (see config_radio.h)
#if TRANSMIT_SIZE_TEST
void appSendLoRaPacketSizeTestPing()
{
    static int nextBodySize = TRANSMIT_SIZE_TEST_BEGIN;
    uint8_t *testmsg = malloc(nextBodySize);
    if (testmsg != NULL) {
        APP_PRINTF("%s TRANSMIT_SIZE_TEST (%d)\r\n", tracePeer(), nextBodySize);
        memset(testmsg, '?', nextBodySize);
        sensorIgnoreTimeWindow();
        sensorSendToGateway(false, testmsg, nextBodySize);
        free(testmsg);
        nextBodySize -= TRANSMIT_SIZE_TEST_DECREMENT;
        if (nextBodySize <= 0) {
            nextBodySize = 0;
        }
    }
}
#endif
