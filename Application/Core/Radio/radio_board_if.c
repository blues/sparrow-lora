// Copyright 2022 Blues Inc.  All rights reserved.
// Use of this source code is governed by licenses granted by the
// copyright holder including that found in the LICENSE file.

// Interface layer to the Radio Board

#include "board.h"

int32_t RBI_Init(void)
{

    // should be calling BSP_RADIO_Init() but not supported by MX

    GPIO_InitTypeDef  gpio_init_structure = {0};

    // Configure the Radio Switch pin
    gpio_init_structure.Pin   = FE_CTRL1_Pin;
    gpio_init_structure.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio_init_structure.Pull  = GPIO_NOPULL;
    gpio_init_structure.Speed = GPIO_SPEED_FREQ_VERY_HIGH;

    HAL_GPIO_Init(FE_CTRL1_GPIO_Port, &gpio_init_structure);

    gpio_init_structure.Pin = FE_CTRL2_Pin;
    HAL_GPIO_Init(FE_CTRL2_GPIO_Port, &gpio_init_structure);

    gpio_init_structure.Pin = FE_CTRL3_Pin;
    HAL_GPIO_Init(FE_CTRL3_GPIO_Port, &gpio_init_structure);

    HAL_GPIO_WritePin(FE_CTRL2_GPIO_Port, FE_CTRL2_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(FE_CTRL1_GPIO_Port, FE_CTRL1_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(FE_CTRL3_GPIO_Port, FE_CTRL3_Pin, GPIO_PIN_RESET);

    return 0;

}

int32_t RBI_DeInit(void)
{

    // Turn off switch
    HAL_GPIO_WritePin(FE_CTRL1_GPIO_Port, FE_CTRL1_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(FE_CTRL2_GPIO_Port, FE_CTRL2_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(FE_CTRL3_GPIO_Port, FE_CTRL3_Pin, GPIO_PIN_RESET);

    // DeInit the Radio Switch pin
    // 2021-09-22 We do NOT want to deinit the gpio's, else the amplifiers
    // would possibly turn on (because of the floating FE_CTRL pins).
#if 0
    HAL_GPIO_DeInit(FE_CTRL1_GPIO_Port, FE_CTRL1_Pin);
    HAL_GPIO_DeInit(FE_CTRL2_GPIO_Port, FE_CTRL2_Pin);
    HAL_GPIO_DeInit(FE_CTRL3_GPIO_Port, FE_CTRL3_Pin);
#endif

    return 0;

}

int32_t RBI_ConfigRFSwitch(RBI_Switch_TypeDef Config)
{

    switch (Config) {
    case RBI_SWITCH_OFF: {
        // Turn off switch
        HAL_GPIO_WritePin(FE_CTRL3_GPIO_Port, FE_CTRL3_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(FE_CTRL1_GPIO_Port, FE_CTRL1_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(FE_CTRL2_GPIO_Port, FE_CTRL2_Pin, GPIO_PIN_RESET);
        break;
    }
    case RBI_SWITCH_RX: {
        // Turns On in Rx Mode the RF Switch
        HAL_GPIO_WritePin(FE_CTRL3_GPIO_Port, FE_CTRL3_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(FE_CTRL1_GPIO_Port, FE_CTRL1_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(FE_CTRL2_GPIO_Port, FE_CTRL2_Pin, GPIO_PIN_RESET);
        break;
    }
    case RBI_SWITCH_RFO_LP: {
        // Turns On in Tx Low Power the RF Switch
        HAL_GPIO_WritePin(FE_CTRL3_GPIO_Port, FE_CTRL3_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(FE_CTRL1_GPIO_Port, FE_CTRL1_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(FE_CTRL2_GPIO_Port, FE_CTRL2_Pin, GPIO_PIN_SET);
        break;
    }
    case RBI_SWITCH_RFO_HP: {
        // Turns On in Tx High Power the RF Switch
        HAL_GPIO_WritePin(FE_CTRL3_GPIO_Port, FE_CTRL3_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(FE_CTRL1_GPIO_Port, FE_CTRL1_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(FE_CTRL2_GPIO_Port, FE_CTRL2_Pin, GPIO_PIN_SET);
        break;
    }
    default:
        break;
    }

    return 0;

}

int32_t RBI_GetTxConfig(void)
{
    return RBI_CONF_RFO;
}

int32_t RBI_GetWakeUpTime(void)
{
    return RF_WAKEUP_TIME;
}

int32_t RBI_IsTCXO(void)
{
    return IS_TCXO_SUPPORTED;
}

int32_t RBI_IsDCDC(void)
{
    return IS_DCDC_SUPPORTED;
}

