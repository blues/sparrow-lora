// Copyright 2022 Blues Inc.  All rights reserved.
// Use of this source code is governed by licenses granted by the
// copyright holder including that found in the LICENSE file.

#include "appdefs.h"

// Initialize applications to be scheduled (enable external project override)
__weak void schedAppInit()
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
