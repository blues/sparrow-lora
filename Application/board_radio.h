// Copyright 2021 Blues Inc.  All rights reserved.
// Use of this source code is governed by licenses granted by the
// copyright holder including that found in the LICENSE file.

// Radio interface board definitions
#pragma once

// RF Oscillator configuration types
#define RBI_CONF_RFO_LP_HP                  0   // RADIO_CONF_RFO_LP_HP
#define RBI_CONF_RFO_LP                     1   // RADIO_CONF_RFO_LP
#define RBI_CONF_RFO_HP                     2   // RADIO_CONF_RFO_HP

// Indicates the type of switch that our hardware is configured for
#define RBI_CONF_RFO                        RBI_CONF_RFO_LP_HP

// Min and maximum powers as implemented by radio_driver.c
#if (RBI_CONF_RFO == RBI_CONF_RFO_LP)
#define RBO_MAX                             14
#define RBO_MIN                             -17
#elif (RBI_CONF_RFO == RBI_CONF_RFO_HP)
#define RBO_MAX                             22
#define RBO_MIN                             -9
#elif (RBI_CONF_RFO == RBI_CONF_RFO_LP_HP)
#define RBO_MAX                             22
#define RBO_MIN                             -17
#endif

// When debugging adaptable transmit power, start the transmit power at a lower level
// so that we see the effects of low power much more quickly.
#ifdef ATP_DEBUG
#undef RBO_MAX
#define RBO_MAX                             8
#endif

// Number of transmit power levels
#define RBO_LEVELS                          (((RBO_MAX)-(RBO_MIN))+1)

// Radio maximum wakeup time (in ms)
#define RF_WAKEUP_TIME                     10U

// Indicates whether or not TCXO is supported by the board
// 0: TCXO not supported
// 1: TCXO supported
#define IS_TCXO_SUPPORTED                   1U

// Indicates whether or not DCDC is supported by the board
// 0: DCDC not supported
// 1: DCDC supported
#define IS_DCDC_SUPPORTED                   0U

typedef enum {
    RBI_SWITCH_OFF    = 0,    // RADIO_SWITCH_OFF
    RBI_SWITCH_RX     = 1,    // RADIO_SWITCH_RX
    RBI_SWITCH_RFO_LP = 2,    // RADIO_SWITCH_RFO_LP
    RBI_SWITCH_RFO_HP = 3,    // RADIO_SWITCH_RFO_HP
} RBI_Switch_TypeDef;

// Radio Interface
int32_t RBI_Init(void);
int32_t RBI_DeInit(void);
int32_t RBI_ConfigRFSwitch(RBI_Switch_TypeDef Config);
int32_t RBI_GetTxConfig(void);
int32_t RBI_GetWakeUpTime(void);
int32_t RBI_IsTCXO(void);
int32_t RBI_IsDCDC(void);

