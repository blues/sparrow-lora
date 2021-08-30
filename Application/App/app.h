// Copyright 2021 Blues Inc.  All rights reserved.
// Use of this source code is governed by licenses granted by the
// copyright holder including that found in the LICENSE file.

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "timer_if.h"
#include "config_sys.h"
#include "config_radio.h"
#include "config_notecard.h"
#include "note.h"

// Task state management
typedef enum {
    UNKNOWN = 0,
    LOWPOWER,
    RX,
    RX_TIMEOUT,
    RX_ERROR,
    TX,
    TX_TIMEOUT,
    TW_OPEN,
} States_t;
extern int64_t appBootMs;
extern bool appIsGateway;
extern uint32_t gatewayBootTime;
extern char ourAddressText[ADDRESS_LEN*3];
extern uint8_t ourAddress[ADDRESS_LEN];
extern uint8_t gatewayAddress[ADDRESS_LEN];
extern uint8_t invalidAddress[ADDRESS_LEN];
extern char sensorName[SENSOR_NAME_MAX];

// So that receiver can access them
extern wireMessageCarrier wireReceivedCarrier;
extern uint32_t wireReceivedLen;
extern uint32_t wireReceiveTimeoutMs;
extern bool wireReceiveSignalValid;
extern int8_t wireReceiveRSSI;
extern int8_t wireReceiveSNR;

// appinit.c
const char *appFirmwareVersion(void);
void appEnterSoftAP(void);
void MX_AppMain(void);
void MX_AppISR(uint16_t GPIO_Pin);

// app.c
void appSetCoreState(States_t newState);
void appTraceWakeup(void);
void appTimerWakeup(void);
void appButtonWakeup(void);
void appGatewayInit(void);
void appGatewayProcess(void);
void appSensorInit(void);
void appSensorProcess(void);
void sensorIgnoreTimeWindow(void);
void sensorSendReqToGateway(J *req, bool replyRequested);
bool appSensorCacheEntry(uint32_t i, uint8_t *address,
                         int8_t *gatewayRSSI, int8_t *gatewaySNR,
                         int8_t *sensorRSSI, int8_t *sensorSNR,
                         int8_t *sensorTXP, int8_t *sensorLTP, uint16_t *sensorMv,
                         uint32_t *lastReceivedTime,
                         uint32_t *requestsProcessed, uint32_t *requestsLost);
void appSensorCacheEntryResetStats(uint32_t index);
void appSendBeaconToGateway(void);
void appSendLoRaPacketSizeTestPing(void);
bool appProcessButton(void);
uint32_t appTransmitWindowWaitMaxSecs(void);
uint32_t appNextTransmitWindowDueSecs(void);

// led.c
void ledSet(void);
void ledReset(void);
void ledWalk(void);
void ledIndicatePairInProgress(bool on);
bool ledIsPairInProgress(void);
bool ledIsPairMandatory(void);
void ledIndicateReceiveInProgress(bool on);
bool ledIsReceiveInProgress(void);
bool ledIsTransmitInProgress(void);
void ledIndicateTransmitInProgress(bool on);
void ledIndicateAck(int flashes);
bool ledDisabled(void);
#define BUTTON_UNCHANGED    0
#define BUTTON_PRESSED      1
#define BUTTON_HOLD_ABORTED 2
#define BUTTON_HELD         3
uint16_t ledButtonCheck(void);

// flash.c
void flashDFUInit(void);
void flashCodeParams(uint8_t **activeBase, uint8_t **dfuBase, uint32_t *maxBytes, uint32_t *maxPages);
void flashConfigFactoryReset(void);
void flashConfigLoad(void);
bool flashConfigUpdate(void);
#define PEER_TYPE_SELF              0x0001
#define PEER_TYPE_GATEWAY           0x0002
#define PEER_TYPE_SENSOR            0x0004
bool flashConfigUpdatePeer(uint16_t peertype, uint8_t *address, uint8_t *key);
bool flashConfigFindPeerByAddress(uint8_t *address, uint16_t *retPeerType, uint8_t *retKey, char *retName);
bool flashConfigFindPeerByType(uint16_t peertype, uint8_t *retAddress, uint8_t *retKey, char *retName);
bool flashConfigUpdatePeerName(uint8_t *address, uint8_t addressLen, char *name);
uint32_t flashConfigPeers(void);
bool flashWrite(uint8_t *flashDest, void *ramSource, uint32_t bytes);

// radioinit.c
void radioInit(void);

// sensor.c
void sensorTimerCancel(void);
void sensorTimerStart(void);
void sensorPoll(void);
void sensorInterrupt(uint16_t interruptType);
void sensorTimerWakeFromISR(void);
void sensorCmd(char *cmd);

// gateway.c
bool gatewayProcessSensorRequest(uint8_t *sensorAddress, uint8_t *req, uint32_t reqLen, uint8_t **rsp, uint32_t *rspLen);
void gatewayInterrupt(uint16_t interruptType);
void gatewayHousekeeping(bool sensorsChanged, uint32_t cachedSensors);
void gatewayHousekeepingDefer(void);
void gatewaySetEnvVarDefaults(void);
void gatewayCmd(char *cmd);

// note.c
bool noteInit(void);
bool noteSetup(void);
void noteSendToGatewayAsync(J *req, bool responseExpected);

// util.c
void utilHTOA8(unsigned char n, char *p);
void utilAddressToText(const uint8_t *address, char *buf, uint32_t buflen);
void extractNameComponents(char *in, char *namebuf, char *olcbuf, uint32_t olcbuflen);

// auth.c
J *authRequest(uint8_t *sensorAddress, char *sensorName, char *sensorLocationOLC, J *req);

// sched.c
void schedInit(void);
void schedDispatchISR(uint16_t pins);
void schedDispatchResponse(J *rsp);
uint32_t schedPoll(void);
void schedDisable(int sensorID);
void schedActivateNow(int sensorID);
bool schedActivateNowFromISR(int sensorID, bool interruptIfActive, int nextState);
void schedSendingRequest(bool responseRequested);
void schedResponseCompleted(J *rsp);
void schedRequestResponseTimeout(void);
void schedRequestResponseTimeoutCheck(void);
void schedRequestCompleted(void);
void schedSetCompletionState(int sensorID, int successState, int errorState);
void schedSetState(int sensorID, int newstate, const char *why);
int schedGetState(int sensorID);
char *schedStateName(int state);
const char *schedSensorName(int sensorID);

// dfuload.c
void dfuLoader(uint8_t *dst, uint8_t *src, uint32_t pages);

// dfu.c
bool noteFirmwareUpdateIfAvailable(void);

// atp.c
int8_t atpPowerLevel(void);
int8_t atpLowestPowerLevel(void);
void atpMatchPowerLevel(int level);
void atpGatewayMessageReceived(int8_t rssi, int8_t snr, int8_t rssiGateway, int8_t snrGateway);
void atpGatewayMessageLost(void);
void atpGatewayMessageSent(void);
void atpSetTxConfig(void);
