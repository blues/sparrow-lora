// Copyright 2022 Blues Inc.  All rights reserved.
// Use of this source code is governed by licenses granted by the
// copyright holder including that found in the LICENSE file.

#include "sensor.h"

// Enable/disable configured sensors
#if SURVEY_MODE
#define USE_PING_TEST               true
#else
#define USE_BME                     true    // true for Reference Sensor
#define USE_PIR                     true    // true for Reference Sensor
#define USE_BUTTON                  true    // button-press sends a message
#define USE_PING_TEST               false
#endif

// Initialize sensors (enable external project override)
__weak void initSensors()
{

    // Handles the Bosch BME280 temp/humidity/pressure sensor
#if USE_BME
    bmeInit();
#endif

    // Handles the Excelitas Passive-Infrared Motion Detector
#if USE_PIR
    pirInit();
#endif

    // Used to support a button press to generate a _health.qo message
#if USE_BUTTON
    buttonInit();
#endif

    // Used to observe comms behavior of a cluster of sensors
#if USE_PING_TEST
    pingInit();
#endif

}
