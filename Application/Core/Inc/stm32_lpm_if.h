// Copyright 2021 Blues Inc.  All rights reserved.
// Use of this source code is governed by licenses granted by the
// copyright holder including that found in the LICENSE file.

// Low Power Manager interface configuration
#pragma once

#include "stm32_lpm.h"

void PWR_EnterOffMode(void);
void PWR_ExitOffMode(void);
void PWR_EnterStopMode(void);
void PWR_ExitStopMode(void);
void PWR_EnterSleepMode(void);
void PWR_ExitSleepMode(void);
