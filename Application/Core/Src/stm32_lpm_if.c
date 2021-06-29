// Copyright 2021 Blues Inc.  All rights reserved.
// Use of this source code is governed by licenses granted by the
// copyright holder including that found in the LICENSE file.

#include "main.h"
#include "stm32_lpm.h"
#include "stm32_lpm_if.h"

// Power driver callbacks handler
const struct UTIL_LPM_Driver_s UTIL_PowerDriver = {
    PWR_EnterSleepMode,
    PWR_ExitSleepMode,

    PWR_EnterStopMode,
    PWR_ExitStopMode,

    PWR_EnterOffMode,
    PWR_ExitOffMode,
};

void PWR_EnterOffMode(void)
{
}

void PWR_ExitOffMode(void)
{
}

void PWR_EnterStopMode(void)
{

    // Suspend
    MX_DBG_Suspend();

    // Suspend sysTick : work around for degugger problem in dual core (tickets 71085,  72038, 71087 )
    HAL_SuspendTick();

    // Clear Status Flag before entering STOP/STANDBY Mode
    LL_PWR_ClearFlag_C1STOP_C1STB();

    HAL_PWREx_EnterSTOP2Mode(PWR_STOPENTRY_WFI);

}

void PWR_ExitStopMode(void)
{

    // Resume sysTick : work around for degugger problem in dual core
    HAL_ResumeTick();

    // Not retained peripherals:
    //    ADC interface
    //    DAC interface USARTx, TIMx, i2Cx, SPIx
    //    SRAM ctrls, DMAx, DMAMux, AES, RNG, HSEM

    // Resume not retained USARTx and DMA
    MX_DBG_Resume();

}

void PWR_EnterSleepMode(void)
{

    // Suspend sysTick
    HAL_SuspendTick();
    HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);

}

void PWR_ExitSleepMode(void)
{

    // Suspend sysTick
    HAL_ResumeTick();

}

