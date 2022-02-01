// Copyright 2022 Blues Inc.  All rights reserved.
// Use of this source code is governed by licenses granted by the
// copyright holder including that found in the LICENSE file.

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Enables trace output and input on an attached serial terminal.  Note that
// if using LPUART1 this costs xxx nA, and using USART2 it's crazy expensive.
// Do NOT ship battery powered products with this enabled.
#define DEBUGGER_ON                                     true

// Enable tracing
#define DEBUGGER_ON_USART2                              false
#define DEBUGGER_ON_LPUART1                             true

// Special GPIO trace methods when working on radio code
#define DEBUGGER_RADIO_DBG_GPIO                         false

// Disable entering STOP2 low-power mode (should never be necessary, even when debugging)
#define LOW_POWER_DISABLE                               false

// Normally, on sensors, the LEDs will shut off after some period of time after
// boot in order to save energy.  Sometimes disabling this feature is useful
// when debugging.  Obviously if in an enclosure where LEDs are not visible
// this should be set to false.
#define LEDS_ALWAYS                                     false

// Verbose level for all trace logs
#define VERBOSE_LEVEL               VLEVEL_M

// Enable trace logs
#define APP_LOG_ENABLED             1

// Trace methods (which are designed this way so they don't require the large printf library)
size_t trace(const char *message);
char *tracePeer(void);
#define DEBUG_VARIABLE(X) ((void)(X))
#if DEBUGGER_ON
void traceClearID(void);
void traceSetID(const char *state, const uint8_t *address, uint32_t requestID);
void traceInput(void);
bool traceInputAvailable(void);
#else
#define traceClearID(void)
#define traceSetID(state, address, requestID)
#define traceInput(void)
#define traceInputAvailable(void) false
#endif
