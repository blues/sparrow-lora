// Copyright 2022 Blues Inc.  All rights reserved.
// Use of this source code is governed by licenses granted by the
// copyright holder including that found in the LICENSE file.

// Log configuration (used by SubGHz_Phy Middleware)
#pragma once

// Enable to see what's happening in the radio stack
//#define MW_LOG_ENABLED

#ifdef MW_LOG_ENABLED
#define MW_LOG(TS,VL, ...)   do{ {printf(__VA_ARGS__);} }while(0)
#else
#define MW_LOG(TS,VL, ...)
#endif
