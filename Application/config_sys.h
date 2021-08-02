// Copyright 2021 Blues Inc.  All rights reserved.
// Use of this source code is governed by licenses granted by the
// copyright holder including that found in the LICENSE file.

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Optionally Low Power mode
#define LOW_POWER_DISABLE 0

// Always enable LEDs, even on sensors with batteries that nobody is debugging
#define LEDS_ALWAYS                                     false

// Enable tracing
#define DEBUGGER_ON                                     1
#define DEBUGGER_ON_USART2                              0
#define DEBUGGER_ON_LPUART1                             1

// Special GPIO trace methods when working on radio code
#define DEBUGGER_RADIO_DBG_GPIO                         0

// Trace methods (which are designed this way so they don't require the large printf library)
size_t trace(const char *message);
#if DEBUGGER_ON
void traceN(const char *message, uint32_t length);
void traceNL(void);
void trace32(uint32_t value);
void traceLn(const char *message);
void trace2Ln(const char *m1, const char *m2);
void trace3Ln(const char *m1, const char *m2, const char *m3);
void traceValueLn(const char *m1, uint32_t n1, const char *m2);
void traceValue2Ln(const char *m1, uint32_t n1, const char *m2, uint32_t n2, const char *m3);
void traceValue3Ln(const char *m1, uint32_t n1, const char *m2, uint32_t n2, const char *m3, uint32_t n3, const char *m4);
void traceValue4Ln(const char *m1, uint32_t n1, const char *m2, uint32_t n2, const char *m3, uint32_t n3, const char *m4, uint32_t n4, const char *m5);
void traceValue5Ln(const char *m1, uint32_t n1, const char *m2, uint32_t n2, const char *m3, uint32_t n3, const char *m4, uint32_t n4, const char *m5, uint32_t n5, const char *m6);
void traceBufferLn(const char *m1, const char *buffer, uint32_t length);
void traceClearID(void);
void traceSetID(const char *state, const uint8_t *address, uint32_t requestID);
void traceInput(void);
bool traceInputAvailable(void);
#else
#define traceN(message, length)
#define traceNL()
#define trace32(value)
#define traceLn(message)
#define traceValueLn(m1, n1, m2)
#define traceValue2Ln(m1, n1, m2, n2, m3)
#define traceValue3Ln(m1, n1, m2, n2, m3, n3, m4)
#define traceValue4Ln(m1, n1, m2, n2, m3, n3, m4, n4, m5)
#define traceValue5Ln(m1, n1, m2, n2, m3, n3, m4, n4, m5, n5, m6)
#define traceBufferLn(m1, buffer, length)
#define traceClearID(void)
#define traceSetID(state, address, requestID)
#define traceInput() (0)
#define traceInputAvailable() (0)
#endif
