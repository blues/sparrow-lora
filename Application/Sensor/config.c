// Copyright 2021 Blues Inc.  All rights reserved.
// Use of this source code is governed by licenses granted by the
// copyright holder including that found in the LICENSE file.

#include "sensor.h"

// Enable/disable configured sensors
#define USE_BME                     false
#define USE_PYQ                     false
#define USE_PING_DEMO               true
#define USE_TASK_SCHEDULER_TEST     false
#define USE_SLEEP_CURRENT_TEST      false

// Convenient ways of converting human-readable units to secons
#define SECS(x)    (x)
#define MINS(x)    (SECS(x)*60)
#define HRS(x)     (MINS(x)*60)
#define DAYS(x)    (HRS(x)*24)

// Main sensor definitions
sensorConfig allSensors[] = {

    // Task serving the Bosch BME280 temperature/humidity/pressure sensor
#if USE_BME
    {
        .name = "bme",
        .activationPeriodSecs = MINS(15),
        .pollIntervalSecs = 15,
        .initFn = bmeInit,
        .activateFn = NULL,
        .interruptFn = NULL,
        .pollFn = bmePoll,
        .responseFn = bmeResponse,
    },
#endif

    // Task serving the Excelitas PYQ 1548/7660 Motion Detector
#if USE_PYQ
    {
        .name = "pyq",
        .activationPeriodSecs = HRS(1),
        .pollIntervalSecs = 15,
        .initFn = pyqInit,
        .activateFn = NULL,
        .interruptFn = pyqISR,
        .pollFn = pyqPoll,
        .responseFn = pyqResponse,
    },
#endif

    // The ping task, when enabled, sends out test messages repeatedly ASAP
#if USE_PING_DEMO
    {
        .name = "ping",
        .activationPeriodSecs = MINS(5),
        .pollIntervalSecs = 15,
        .initFn = NULL,
        .activateFn = NULL,
        .interruptFn = pingISR,
        .pollFn = pingPoll,
        .responseFn = pingResponse,
    },
#endif

    // The null task's purpose is simply to demonstrate the task scheduler
#if USE_TASK_SCHEDULER_TEST
    {
        .name = "null30",
        .activationPeriodSecs = 30,
        .pollIntervalSecs = 5,
        .initFn = NULL,
        .activateFn = NULL,
        .interruptFn = NULL,
        .pollFn = nullPoll,
        .responseFn = NULL,
    },
    {
        .name = "null60",
        .activationPeriodSecs = 60,
        .pollIntervalSecs = 5,
        .initFn = NULL,
        .activateFn = NULL,
        .interruptFn = NULL,
        .pollFn = nullPoll,
        .responseFn = NULL,
    },
    {
        .name = "null120",
        .activationPeriodSecs = 120,
        .pollIntervalSecs = 5,
        .initFn = NULL,
        .activateFn = NULL,
        .interruptFn = NULL,
        .pollFn = nullPoll,
        .responseFn = NULL,
    },
#endif

    // The sleep current task's purpose is to sleep for a long time so current
    // drawn by a board that is neither transmitting or receiving can be measured.
#if USE_SLEEP_CURRENT_TEST
    {
        .name = "sleep",
        .activationPeriodSecs = 86400,
        .pollIntervalSecs = 5,
        .initFn = NULL,
        .activateFn = NULL,
        .interruptFn = NULL,
        .pollFn = nullPoll,
        .responseFn = NULL,
    },
#endif

};

// Enable the sensor scheduler to find the config
uint32_t sensorGetConfig(sensorConfig **retSensorConfig)
{
    if (retSensorConfig != NULL) {
        *retSensorConfig = allSensors;
    }
    return (sizeof(allSensors) / sizeof(allSensors[0]));
}
