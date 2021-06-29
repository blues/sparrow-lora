// Copyright 2021 Blues Inc.  All rights reserved.
// Use of this source code is governed by licenses granted by the
// copyright holder including that found in the LICENSE file.

#pragma once

#include "board.h"
#include "app.h"
#include "note.h"

// Forward reference to sensor configuration definition, because circular.
struct sensorConfig_c;

// Called once so that sensor may to state initialization, etc. Note that
// this method must not send messages to the gateway; it's only allowed
// to do local operations.
typedef bool (*sensorInitFunc) (int sensorID);

// Called when time to activate; return false to cancel this activation.
// Note that this method must not send messages to the gateway; it's only
// allowed to do local operations.
typedef bool (*sensorActivateFunc) (int sensorID);

// Called repeatedly while activate; set to STATE_INACTIVE to deactivate.
// This function implements the sensor's state machine, where negative
// states are reserved. When STATE_INACTIVE is set by this function,
// the sensor is deactivated.
#define STATE_UNDEFINED             -1
#define STATE_ONCE                  -2
#define STATE_ACTIVATED             -3
#define STATE_DEACTIVATED           -4
#define STATE_SENDING_REQUEST       -5
#define STATE_RECEIVING_RESPONSE    -6
typedef void (*sensorPollFunc) (int sensorID, int state);

// Called when a sensor does a notecard request and asynchronously receives
// a reply.  This will be called when a response comes back or when it
// times out; if timeout the "rsp" field will be null.
typedef void (*sensorResponseFunc) (int sensorID, J *rsp);

// An ISR that is called on ANY+ALL interrupts; pins indicates exti lines that changed.
typedef void (*sensorInterruptFunc) (int sensorID, uint16_t pins);

// Sensor Configuration definition
struct sensorConfig_c {

    // General
    const char *name;

    // How often we get activated
    uint32_t activationPeriodSecs;

    // While sensor is active, how often it's polled
    uint32_t pollIntervalSecs;

    // Handlers
    sensorInitFunc initFn;
    sensorActivateFunc activateFn;
    sensorInterruptFunc interruptFn;
    sensorPollFunc pollFn;
    sensorResponseFunc responseFn;

};
typedef struct sensorConfig_c sensorConfig;

// config.c
uint32_t sensorGetConfig(sensorConfig **retSensorConfig);

// ping.c
void pingISR(int sensorID, uint16_t pins);
bool pingActivate(int sensorID);
void pingPoll(int sensorID, int state);
void pingResponse(int sensorID, J *rsp);

// null.c
bool nullActivate(int sensorID);
void nullPoll(int sensorID, int state);

// pyq.c
bool pyqInit(int sensorID);
void pyqISR(int sensorID, uint16_t pins);
bool pyqActivate(int sensorID);
void pyqPoll(int sensorID, int state);
void pyqResponse(int sensorID, J *rsp);

// bme.c
bool bmeInit(int sensorID);
bool bmeActivate(int sensorID);
void bmePoll(int sensorID, int state);
void bmeResponse(int sensorID, J *rsp);
