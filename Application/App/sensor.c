// Copyright 2021 Blues Inc.  All rights reserved.
// Use of this source code is governed by licenses granted by the
// copyright holder including that found in the LICENSE file.

#include "main.h"
#include "stm32_timer.h"
#include "app.h"

// Sensor Sleep Timer
#define sensorSleepMaxSecs          (60*60)         // Wake up at least hourly
static UTIL_TIMER_Object_t sensorSleepTimer;
uint32_t sensorWorkDueTime = 0;                     // Time of next work that is due for sensor data

// Forwards
void sensorTimerEvent(void *context);

// Process the timed event
void sensorTimerEvent(void *context)
{
    appTimerWakeup();
}

// Set an immediate timer to wake up now
void sensorTimerWakeFromISR()
{
    sensorTimerCancel();
    sensorWorkDueTime = 0;
    UTIL_TIMER_Create(&sensorSleepTimer, 0xFFFFFFFFU, UTIL_TIMER_ONESHOT, sensorTimerEvent, NULL);
    UTIL_TIMER_SetPeriod(&sensorSleepTimer, 1);
    UTIL_TIMER_Start(&sensorSleepTimer);
}

// Place the CPU into a sleep mode until the next event occurs
void sensorTimerStart()
{

    // Cancel any pending timer
    sensorTimerCancel();

    // Wait a moment to allow any pending I/O to be completed (such as debugger output)
    HAL_Delay(500);

    // Compute the sleep time based on when our work polling is due
    uint32_t thisSleepSecs = sensorSleepMaxSecs;

    // Minimize it based on whether or not we are beaconing for pairing
    if (ledIsPairInProgress()) {
        if (thisSleepSecs > PAIRING_BEACON_SECS) {
            thisSleepSecs = PAIRING_BEACON_SECS;
        }
    }

    // Minimize the sleep based on how often we should do sensor data-related work
    uint32_t now = NoteTimeST();
    uint32_t sensorWakeupSecs = 1;
    if (sensorWorkDueTime >= now) {
        sensorWakeupSecs = sensorWorkDueTime - now;
    }
    if (sensorWakeupSecs < thisSleepSecs) {
        thisSleepSecs = sensorWakeupSecs;
    }

    // Schedule the timer
    if (thisSleepSecs > 1) {
        uint32_t transmitWindowDueSecs = appNextTransmitWindowDueSecs();
        if (transmitWindowDueSecs == 0) {
            traceValueLn("sched: sleeping ", thisSleepSecs, "s");
        } else {
            traceValue2Ln("sched: sleeping ", thisSleepSecs, "s (next transmit window in ", transmitWindowDueSecs, "s)");
        }
    }

    // Put the radio to sleep if we're waiting for a while.  We choose 5 seconds because it's less than the amount
    // of time we'll wait if sleeping waiting for a transmit window.
    if (thisSleepSecs > 5 && radioDeepSleep()) {
        traceLn("sched: radio put into deep sleep mode");
    }

    // Go to sleep
    UTIL_TIMER_Create(&sensorSleepTimer, 0xFFFFFFFFU, UTIL_TIMER_ONESHOT, sensorTimerEvent, NULL);
    UTIL_TIMER_SetPeriod(&sensorSleepTimer, thisSleepSecs*1000);
    UTIL_TIMER_Start(&sensorSleepTimer);

}

// Cancel any pending sensor timer
void sensorTimerCancel()
{
    UTIL_TIMER_Stop(&sensorSleepTimer);
}

// Poll for something worth doing, and call sensorSendToGateway or sensorSendReqToGateway
void sensorPoll()
{

    // Time-out any requests or responses that may have been pending
    schedRequestResponseTimeoutCheck();

    // Poll the sensor scheduler and restart the timer
    // be used in setTime
    sensorWorkDueTime = schedPoll();
    sensorTimerStart();

    // Send a pairing beacon to the listening gateway
    if (ledIsPairInProgress()) {
        appSendBeaconToGateway();
        return;
    }

}

// Execute console command
void sensorCmd(char *cmd)
{
    trace("??");
    traceNL();
}
