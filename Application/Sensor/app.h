// Copyright 2022 Blues Inc.  All rights reserved.
// Use of this source code is governed by licenses granted by the
// copyright holder including that found in the LICENSE file.

#pragma once

// Standard Libraries
#include <stdbool.h>
#include <stdint.h>

// 3rd-party Libraries
#include <note.h>

// Internal Headers
#include "board.h"
#include "framework.h"

// If TRUE, we're in survey mode in which the button is used to send
// pings that include RSSI/SNR transmitted at full power, and all
// scheduled activities are disabled.
#define SURVEY_MODE                 false

// Forward reference to sensor configuration definition, because circular.
struct sensorConfig_c;

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
    sensorActivateFunc activateFn;
    sensorInterruptFunc interruptFn;
    sensorPollFunc pollFn;
    sensorResponseFunc responseFn;

};

// init.c
void initApps(void);

// Sensor init methods
bool bmeInit(void);
bool pirInit(void);
bool pingInit(void);
bool buttonInit(void);
