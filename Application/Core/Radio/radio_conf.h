// Copyright 2022 Blues Inc.  All rights reserved.
// Use of this source code is governed by licenses granted by the
// copyright holder including that found in the LICENSE file.

// Radio configuration
#pragma once

#include "stm32wlxx_ll_gpio.h"
#include "main.h"
#include "stm32_mem.h"
#include "mw_log_conf.h"
#include "board_radio.h"
#include "utilities_def.h"
#include "config_sys.h"

// Set RX pin to high or low level
#define DBG_GPIO_RADIO_RX(set_rst) DBG_GPIO_##set_rst##_LINE(DBG_LINE1_GPIO_Port, DBG_LINE1_Pin);

// Set TX pin to high or low level
#define DBG_GPIO_RADIO_TX(set_rst) DBG_GPIO_##set_rst##_LINE(DBG_LINE2_GPIO_Port, DBG_LINE2_Pin);

// Max payload buffer size
#define RADIO_RX_BUF_SIZE          255

// Drive value used anytime radio is NOT in TX low power mode
#define SMPS_DRIVE_SETTING_DEFAULT  SMPS_DRV_40

// Drive value used anytime radio is in TX low power mode
//        TX low power mode is the worst case because the PA sinks from SMPS
//        while in high power mode, current is sunk directly from the battery
#define SMPS_DRIVE_SETTING_MAX      SMPS_DRV_60

// Provides the frequency of the chip running on the radio and the frequency step
// These defines are used for computing the frequency divider to set the RF frequency
// Note: override the default configuration of radio_driver.c
#define XTAL_FREQ                   ( 32000000UL )

// In XO mode, set internal capacitor (from 0x00 to 0x2F starting 11.2pF with 0.47pF steps)
#define XTAL_DEFAULT_CAP_VALUE      0x20

// Frequency error (in Hz) can be compensated here.
// warning XO frequency error generates (de)modulator sampling time error which can not be compensated
#define RF_FREQUENCY_ERROR          ((int32_t) 0)

// Voltage of TCXO's VDD supply
#if (CURRENT_BOARD == BOARD_NUCLEO)
#define TCXO_CTRL_VOLTAGE           TCXO_CTRL_1_7V        // NT2016SF-32M-END5875A
#elif (CURRENT_BOARD == BOARD_WIO_E5)
#define TCXO_CTRL_VOLTAGE           TCXO_CTRL_1_7V        // Voltage across Vss and Vdd_TCX0
#else
#define TCXO_CTRL_VOLTAGE           TCXO_CTRL_3_0V        // TYETBCSANF-32.000000
#endif

// Radio maximum wakeup time (in ms)
// override the default configuration of radio_driver.c
// #define RF_WAKEUP_TIME              ( 1UL )

#ifndef CRITICAL_SECTION_BEGIN
// Macro used to enter the critical section
#define CRITICAL_SECTION_BEGIN( )      UTILS_ENTER_CRITICAL_SECTION( )
#endif /* !CRITICAL_SECTION_BEGIN */
#ifndef CRITICAL_SECTION_END
// Macro used to exit the critical section
#define CRITICAL_SECTION_END( )        UTILS_EXIT_CRITICAL_SECTION( )
#endif /* !CRITICAL_SECTION_END */

// Function mapping
// SUBGHZ interface init to radio Middleware
#define RADIO_INIT                              MX_SUBGHZ_Init
#define RADIO_DEINIT                            MX_SUBGHZ_DeInit

// Delay interface to radio Middleware
#define RADIO_DELAY_MS                          HAL_Delay

// Memset utilities interface to radio Middleware
#define RADIO_MEMSET8( dest, value, size )      UTIL_MEM_set_8( dest, value, size )

// Memcpy utilities interface to radio Middleware
#define RADIO_MEMCPY8( dest, src, size )        UTIL_MEM_cpy_8( dest, src, size )
