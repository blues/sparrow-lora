// Copyright 2021 Blues Inc.  All rights reserved.
// Use of this source code is governed by licenses granted by the
// copyright holder including that found in the LICENSE file.

#pragma once

#include "stm32_timer.h"
#include "stm32_systime.h"

/**
  * Init RTC hardware
  * ReturnsStatus based on @ref UTIL_TIMER_Status_t
  */
UTIL_TIMER_Status_t TIMER_IF_Init(void);

/**
  * Set the alarm
  * The alarm is set at timeout from timer Reference (TimerContext)
  * Input: timeout Duration of the Timer in ticks
  * ReturnsStatus based on @ref UTIL_TIMER_Status_t
  */
UTIL_TIMER_Status_t TIMER_IF_StartTimer(uint32_t timeout);

/**
  * Stop the Alarm
  * ReturnsStatus based on @ref UTIL_TIMER_Status_t
  */
UTIL_TIMER_Status_t TIMER_IF_StopTimer(void);

/**
  * set timer Reference (TimerContext)
  * Returns Timer Reference Value in  Ticks
  */
uint32_t TIMER_IF_SetTimerContext(void);

/**
  * Get the RTC timer Reference
  * ReturnsTimer Value in  Ticks
  */
uint32_t TIMER_IF_GetTimerContext(void);

/**
  * Get the timer elapsed time since timer Reference (TimerContext) was set
  * ReturnsRTC Elapsed time in ticks
  */
uint32_t TIMER_IF_GetTimerElapsedTime(void);

/**
  * Get the timer value
  * ReturnsRTC Timer value in ticks
  */
uint32_t TIMER_IF_GetTimerValue(void);

/**
  * Return the minimum timeout in ticks the RTC is able to handle
  * Returnsminimum value for a timeout in ticks
  */
uint32_t TIMER_IF_GetMinimumTimeout(void);

/**
  * a delay of delay ms by polling RTC
  * Input: delay in ms
  */
void TIMER_IF_DelayMs(uint32_t delay);

/**
  * converts time in ms to time in ticks
  * Input: timeMilliSec time in milliseconds
  * Returnstime in timer ticks
  */
uint32_t TIMER_IF_Convert_ms2Tick(uint32_t timeMilliSec);

/**
  * converts time in ticks to time in ms
  * Input: tick time in timer ticks
  * Returns time in timer milliseconds
  */
uint32_t TIMER_IF_Convert_Tick2ms(uint32_t tick);

/**
  * Get rtc time
  * Out: subSeconds in milliseconds
  * Returns time seconds
  */
uint32_t TIMER_IF_GetTime(uint16_t *milliseconds);

/**
  * Get rtc time in milliseconds
  * Returns time milliseconds
  */
int64_t TIMER_IF_GetTimeMs(void);

/**
  * write seconds in backUp register
  * Used to store seconds difference between RTC time and Unix time
  * Input: Seconds time in seconds
  */
void TIMER_IF_BkUp_Write_Seconds(uint32_t Seconds);

/**
  * reads seconds from backUp register
  * Used to store seconds difference between RTC time and Unix time
  * ReturnsTime in seconds
  */
uint32_t TIMER_IF_BkUp_Read_Seconds(void);

/**
  * writes SubSeconds in backUp register
  * Used to store SubSeconds difference between RTC time and Unix time
  * @param[in] SubSeconds time in SubSeconds
  */
void TIMER_IF_BkUp_Write_SubSeconds(uint32_t SubSeconds);

/**
  * reads SubSeconds from backUp register
  * Used to store SubSeconds difference between RTC time and Unix time
  * ReturnsTime in SubSeconds
  */
uint32_t TIMER_IF_BkUp_Read_SubSeconds(void);
