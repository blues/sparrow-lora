// Copyright 2021 Blues Inc.  All rights reserved.
// Use of this source code is governed by licenses granted by the
// copyright holder including that found in the LICENSE file.

// Definitions for ST utilities
#pragma once

// LOW POWER MANAGER

// Supported requester to the MCU Low Power Manager - can be increased up  to 32
// It lists a bit mapping of all users of the Low Power Manager
typedef enum {
    CFG_LPM_APPLI_Id,
    CFG_LPM_UART_TX_Id,
    CFG_LPM_TCXO_WA_Id,

} CFG_LPM_Id_t;

// SEQUENCER

// This is the list of priority required by the application
// Each Id shall be in the range 0..31
typedef enum {
    CFG_SEQ_Prio_0,

    CFG_SEQ_Prio_NBR,
} CFG_SEQ_Prio_Id_t;

// This is the list of task id required by the application
// Each Id shall be in the range 0..31
typedef enum {
    CFG_SEQ_Task_Sparrow_Process,

    CFG_SEQ_Task_NBR
} CFG_SEQ_Task_Id_t;

