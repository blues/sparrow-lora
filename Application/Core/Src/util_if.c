// Copyright 2021 Blues Inc.  All rights reserved.
// Use of this source code is governed by licenses granted by the
// copyright holder including that found in the LICENSE file.

#include "main.h"
#include "stm32_seq.h"
#include "stm32_lpm.h"
#include "stm32_timer.h"
#include "stm32_adv_trace.h"
#include "utilities_def.h"

// Initializes ST's utility packages
void MX_UTIL_Init(void)
{

    // Initialises timer and RTC
    UTIL_TIMER_Init();

    // Initialize the trace terminal
    UTIL_ADV_TRACE_Init();
    UTIL_ADV_TRACE_SetVerboseLevel(VERBOSE_LEVEL);

    // Init low power manager
    UTIL_LPM_Init();
    // Disable Stand-by mode
    UTIL_LPM_SetOffMode((1 << CFG_LPM_APPLI_Id), UTIL_LPM_DISABLE);

#if defined (LOW_POWER_DISABLE) && (LOW_POWER_DISABLE == 1)
    // Disable Stop Mode
    UTIL_LPM_SetStopMode((1 << CFG_LPM_APPLI_Id), UTIL_LPM_DISABLE);
#elif !defined (LOW_POWER_DISABLE)
#error LOW_POWER_DISABLE not defined
#endif // LOW_POWER_DISABLE

}

// Redefines __weak function in stm32_seq.c such to enter low power
void UTIL_SEQ_Idle(void)
{
    UTIL_LPM_EnterLowPower();
}
