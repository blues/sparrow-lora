// Copyright 2022 Blues Inc.  All rights reserved.
// Use of this source code is governed by licenses granted by the
// copyright holder including that found in the LICENSE file.

#include <stdbool.h>
#include "note.h"

#pragma once

// Called when time to activate; return false to cancel this activation.
// Note that this method must not send messages to the gateway; it's only
// allowed to do local operations.
typedef bool (*schedActivateFunc) (int appID);

// Called repeatedly while activate; set to STATE_INACTIVE to deactivate.
// This function implements the app's state machine, where negative
// states are reserved. When STATE_INACTIVE is set by this function,
// the app is deactivated.
#define STATE_UNDEFINED             -1
#define STATE_ONCE                  -2
#define STATE_ACTIVATED             -3
#define STATE_DEACTIVATED           -4
#define STATE_SENDING_REQUEST       -5
#define STATE_RECEIVING_RESPONSE    -6
typedef void (*schedPollFunc) (int appID, int state);

// Called when an app does a notecard request and asynchronously receives
// a reply.  This will be called when a response comes back or when it
// times out; if timeout the "rsp" field will be null.
typedef void (*schedResponseFunc) (int appID, J *rsp);

// An ISR that is called on ANY+ALL interrupts; pins indicates exti lines that changed.
typedef void (*schedInterruptFunc) (int appID, uint16_t pins);

// App Configuration definition
typedef struct {

    // General
    const char *name;

    // How often we get activated
    uint32_t activationPeriodSecs;

    // While app is active, how often it's polled
    uint32_t pollIntervalSecs;

    // Handlers
    schedActivateFunc activateFn;
    schedInterruptFunc interruptFn;
    schedPollFunc pollFn;
    schedResponseFunc responseFn;

} schedAppConfig;

// init.c
void schedAppInit(void);

// sched.c
void schedActivateNow(int appID);
bool schedActivateNowFromISR(int appID, bool interruptIfActive, int nextState);
const char *schedAppName(int appID);
void schedDisable(int appID);
void schedDispatchISR(uint16_t pins);
void schedDispatchResponse(J *rsp);
int schedGetState(int appID);
void schedInit(void);
bool schedIsActive(int appID);
uint32_t schedPoll(void);
int schedRegisterApp(schedAppConfig *sensorToRegister);
void schedRequestCompleted(void);
void schedRequestResponseTimeout(void);
void schedRequestResponseTimeoutCheck(void);
void schedResponseCompleted(J *rsp);
void schedSendingRequest(bool responseRequested);
void schedSetCompletionState(int appID, int successState, int errorState);
void schedSetState(int appID, int newstate, const char *why);
char *schedStateName(int state);
