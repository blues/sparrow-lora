// Copyright 2022 Blues Inc.  All rights reserved.
// Use of this source code is governed by licenses granted by the
// copyright holder including that found in the LICENSE file.

// Timer server definitions (used by SubGHz_Phy Middleware)
#pragma once

#include "stm32_timer.h"

// Max timer mask
#define TIMERTIME_T_MAX ( ( uint32_t )~0 )

// Timer value on 32 bits
#define TimerTime_t UTIL_TIMER_Time_t

// Timer object description
#define TimerEvent_t UTIL_TIMER_Object_t

// Create the timer object
#define TimerInit(HANDLE, CB) do {\
                                   UTIL_TIMER_Create( HANDLE, TIMERTIME_T_MAX, UTIL_TIMER_ONESHOT, CB, NULL);\
                                 } while(0)

// Update the period and start the timer
#define TimerSetValue(HANDLE, TIMEOUT) do{ \
                                           UTIL_TIMER_SetPeriod(HANDLE, TIMEOUT);\
                                         } while(0)

// Start and adds the timer object to the list of timer events
#define TimerStart(HANDLE)   do {\
                                  UTIL_TIMER_Start(HANDLE);\
                                } while(0)

// Stop and removes the timer object from the list of timer events
#define TimerStop(HANDLE)   do {\
                                 UTIL_TIMER_Stop(HANDLE);\
                               } while(0)

// Return the current time
#define TimerGetCurrentTime  UTIL_TIMER_GetCurrentTime

// Return the elapsed time
#define TimerGetElapsedTime UTIL_TIMER_GetElapsedTime

