// Copyright 2022 Blues Inc.  All rights reserved.
// Use of this source code is governed by licenses granted by the
// copyright holder including that found in the LICENSE file.

#include "framework.h"

#pragma once

// Enable/disable configured apps
#define USE_BME                     true    // true for Reference sensor
#define USE_PIR                     true    // true for Reference sensor
#define USE_BUTTON                  true    // button-press sends a message
#define USE_PING_TEST               false   // for testing & locating sensors

// App init methods
bool bmeInit(void);
bool pirInit(void);
bool pingInit(void);
bool buttonInit(void);
