// Copyright 2021 Blues Inc.  All rights reserved.
// Use of this source code is governed by licenses granted by the
// copyright holder including that found in the LICENSE file.

#include "main.h"

extern DMA_HandleTypeDef hdma_adc;
extern DMA_HandleTypeDef hdma_i2c2_rx;
extern DMA_HandleTypeDef hdma_i2c2_tx;
extern DMA_HandleTypeDef hdma_spi1_rx;
extern DMA_HandleTypeDef hdma_spi1_tx;
extern DMA_HandleTypeDef hdma_usart1_rx;
extern DMA_HandleTypeDef hdma_usart1_tx;
extern DMA_HandleTypeDef hdma_usart2_rx;
extern DMA_HandleTypeDef hdma_usart2_tx;

// Initializes the Global MSP.
void HAL_MspInit(void)
{
}

// TIM17 Init
void HAL_TIM_Base_MspInit(TIM_HandleTypeDef* htim_base)
{
  if (htim_base->Instance==TIM17) {
    __HAL_RCC_TIM17_CLK_ENABLE();
  }
}

// TIM17 DeInit
void HAL_TIM_Base_MspDeInit(TIM_HandleTypeDef* htim_base)
{
  if (htim_base->Instance==TIM17) {
    __HAL_RCC_TIM17_CLK_DISABLE();
  }

}

// ADC MSP Init
void HAL_ADC_MspInit(ADC_HandleTypeDef* hadc)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (hadc->Instance==ADC) {

        // Configures the peripherals clock
        RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
        PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_ADC;
        PeriphClkInitStruct.AdcClockSelection = RCC_ADCCLKSOURCE_SYSCLK;
        if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) {
            Error_Handler();
        }

        // Peripheral clock enable
        __HAL_RCC_ADC_CLK_ENABLE();

        // GPIO
        GPIO_InitStruct.Pin = A0_Pin;
        GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(A0_GPIO_Port, &GPIO_InitStruct);
        GPIO_InitStruct.Pin = A1_Pin;
        HAL_GPIO_Init(A1_GPIO_Port, &GPIO_InitStruct);
        GPIO_InitStruct.Pin = A2_Pin;
        HAL_GPIO_Init(A2_GPIO_Port, &GPIO_InitStruct);
        GPIO_InitStruct.Pin = A3_Pin;
        HAL_GPIO_Init(A3_GPIO_Port, &GPIO_InitStruct);

        // DMA
        hdma_adc.Instance = ADC_DMA_Channel;
        hdma_adc.Init.Request = DMA_REQUEST_ADC;
        hdma_adc.Init.Direction = DMA_PERIPH_TO_MEMORY;
        hdma_adc.Init.PeriphInc = DMA_PINC_DISABLE;
        hdma_adc.Init.MemInc = DMA_MINC_ENABLE;
        hdma_adc.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
        hdma_adc.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
        hdma_adc.Init.Mode = DMA_NORMAL;
        hdma_adc.Init.Priority = DMA_PRIORITY_LOW;
        if (HAL_DMA_Init(&hdma_adc) != HAL_OK) {
            Error_Handler();
        }
        __HAL_LINKDMA(hadc, DMA_Handle, hdma_adc);

        // Enable interrupts
        HAL_NVIC_SetPriority(ADC_IRQn, 2, 0);
        HAL_NVIC_EnableIRQ(ADC_IRQn);

    }

}

// ADC Deinitialization
void HAL_ADC_MspDeInit(ADC_HandleTypeDef* hadc)
{

    if(hadc->Instance==ADC) {

        // Clock
        __HAL_RCC_ADC_CLK_DISABLE();

        // GPIO
        HAL_GPIO_DeInit(A0_GPIO_Port, A0_Pin);
        HAL_GPIO_DeInit(A1_GPIO_Port, A1_Pin);
        HAL_GPIO_DeInit(A2_GPIO_Port, A2_Pin);
        HAL_GPIO_DeInit(A3_GPIO_Port, A3_Pin);

        // DMA
        HAL_DMA_DeInit(hadc->DMA_Handle);

        // Interrupts
        HAL_NVIC_DisableIRQ(ADC_IRQn);

    }

}

// I2C MSP Initialization
// This function configures the hardware resources used in this example
void HAL_I2C_MspInit(I2C_HandleTypeDef* hi2c)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

    if (hi2c->Instance==I2C2) {

        // Initializes the peripherals clocks
        PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_I2C2;
        PeriphClkInitStruct.I2c2ClockSelection = RCC_I2C2CLKSOURCE_PCLK1;
        if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) {
            Error_Handler();
        }

        // I2C2 GPIO Configuration
        GPIO_InitStruct.Pin = I2C2_SCL_Pin;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        GPIO_InitStruct.Alternate = I2C2_SCL_GPIO_AF;
        HAL_GPIO_Init(I2C2_SCL_GPIO_Port, &GPIO_InitStruct);
        GPIO_InitStruct.Pin = I2C2_SDA_Pin;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        GPIO_InitStruct.Alternate = I2C2_SDA_GPIO_AF;
        HAL_GPIO_Init(I2C2_SDA_GPIO_Port, &GPIO_InitStruct);

        // Peripheral clock enable
        __HAL_RCC_I2C2_CLK_ENABLE();

        // RX DMA
        hdma_i2c2_rx.Instance = I2C2_RX_DMA_Channel;
        hdma_i2c2_rx.Init.Request = DMA_REQUEST_I2C2_RX;
        hdma_i2c2_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
        hdma_i2c2_rx.Init.PeriphInc = DMA_PINC_DISABLE;
        hdma_i2c2_rx.Init.MemInc = DMA_MINC_ENABLE;
        hdma_i2c2_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hdma_i2c2_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
        hdma_i2c2_rx.Init.Mode = DMA_NORMAL;
        hdma_i2c2_rx.Init.Priority = DMA_PRIORITY_LOW;
        if (HAL_DMA_Init(&hdma_i2c2_rx) != HAL_OK) {
            Error_Handler();
        }
        __HAL_LINKDMA(hi2c,hdmarx,hdma_i2c2_rx);

        // TX DMA
        hdma_i2c2_tx.Instance = I2C2_TX_DMA_Channel;
        hdma_i2c2_tx.Init.Request = DMA_REQUEST_I2C2_TX;
        hdma_i2c2_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
        hdma_i2c2_tx.Init.PeriphInc = DMA_PINC_DISABLE;
        hdma_i2c2_tx.Init.MemInc = DMA_MINC_ENABLE;
        hdma_i2c2_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hdma_i2c2_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
        hdma_i2c2_tx.Init.Mode = DMA_NORMAL;
        hdma_i2c2_tx.Init.Priority = DMA_PRIORITY_LOW;
        if (HAL_DMA_Init(&hdma_i2c2_tx) != HAL_OK) {
            Error_Handler();
        }
        __HAL_LINKDMA(hi2c,hdmatx,hdma_i2c2_tx);

        // Interrupts
        HAL_NVIC_SetPriority(I2C2_EV_IRQn, 2, 0);
        HAL_NVIC_EnableIRQ(I2C2_EV_IRQn);
        HAL_NVIC_SetPriority(I2C2_ER_IRQn, 2, 0);
        HAL_NVIC_EnableIRQ(I2C2_ER_IRQn);

    }

}

// I2C MSP De-Initialization
// This function freeze the hardware resources used in this example
void HAL_I2C_MspDeInit(I2C_HandleTypeDef* hi2c)
{

    if (hi2c->Instance==I2C2) {

        // Peripheral clock disable
        __HAL_RCC_I2C2_CLK_DISABLE();

        // GPIO Disable
        HAL_GPIO_DeInit(I2C2_SDA_GPIO_Port, I2C2_SDA_Pin);
        HAL_GPIO_DeInit(I2C2_SCL_GPIO_Port, I2C2_SCL_Pin);

        // DMA Disable
        HAL_DMA_DeInit(hi2c->hdmarx);
        HAL_DMA_DeInit(hi2c->hdmatx);

        // Interrupt disable
        HAL_NVIC_DisableIRQ(I2C2_EV_IRQn);
        HAL_NVIC_DisableIRQ(I2C2_ER_IRQn);

    }

}

// SPI MSP Init
void HAL_SPI_MspInit(SPI_HandleTypeDef* hspi)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (hspi->Instance==SPI1) {

        // Peripheral clock enable
        __HAL_RCC_SPI1_CLK_ENABLE();

        // GPIO
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        GPIO_InitStruct.Alternate = SPI1_GPIO_AF;
        GPIO_InitStruct.Pin = SPI1_SCK_Pin;
        HAL_GPIO_Init(SPI1_SCK_GPIO_Port, &GPIO_InitStruct);
        GPIO_InitStruct.Pin = SPI1_MOSI_Pin;
        HAL_GPIO_Init(SPI1_MOSI_GPIO_Port, &GPIO_InitStruct);
        GPIO_InitStruct.Pin = SPI1_MOSI_Pin;
        HAL_GPIO_Init(SPI1_SCK_GPIO_Port, &GPIO_InitStruct);
        GPIO_InitStruct.Pin = SPI1_CS_Pin;
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
        HAL_GPIO_Init(SPI1_CS_GPIO_Port, &GPIO_InitStruct);
        HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, GPIO_PIN_SET);

        // SPI1 RX DMA
        hdma_spi1_rx.Instance = SPI1_RX_DMA_Channel;
        hdma_spi1_rx.Init.Request = DMA_REQUEST_SPI1_RX;
        hdma_spi1_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
        hdma_spi1_rx.Init.PeriphInc = DMA_PINC_DISABLE;
        hdma_spi1_rx.Init.MemInc = DMA_MINC_ENABLE;
        hdma_spi1_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hdma_spi1_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
        hdma_spi1_rx.Init.Mode = DMA_NORMAL;
        hdma_spi1_rx.Init.Priority = DMA_PRIORITY_LOW;
        if (HAL_DMA_Init(&hdma_spi1_rx) != HAL_OK) {
            Error_Handler();
        }
        __HAL_LINKDMA(hspi,hdmarx,hdma_spi1_rx);

        // SPI1 TX DMA
        hdma_spi1_tx.Instance = SPI1_TX_DMA_Channel;
        hdma_spi1_tx.Init.Request = DMA_REQUEST_SPI1_TX;
        hdma_spi1_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
        hdma_spi1_tx.Init.PeriphInc = DMA_PINC_DISABLE;
        hdma_spi1_tx.Init.MemInc = DMA_MINC_ENABLE;
        hdma_spi1_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hdma_spi1_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
        hdma_spi1_tx.Init.Mode = DMA_NORMAL;
        hdma_spi1_tx.Init.Priority = DMA_PRIORITY_LOW;
        if (HAL_DMA_Init(&hdma_spi1_tx) != HAL_OK) {
            Error_Handler();
        }
        __HAL_LINKDMA(hspi,hdmatx,hdma_spi1_tx);

        // Interrupts
        HAL_NVIC_SetPriority(SPI1_IRQn, 2, 0);
        HAL_NVIC_EnableIRQ(SPI1_IRQn);

    }

}

// SPI MSP Deinitialization
void HAL_SPI_MspDeInit(SPI_HandleTypeDef* hspi)
{

    if(hspi->Instance==SPI1) {

        // Peripheral clock
        __HAL_RCC_SPI1_CLK_DISABLE();

        // GPIO
        HAL_GPIO_DeInit(SPI1_CS_GPIO_Port, SPI1_CS_Pin);
        HAL_GPIO_DeInit(SPI1_SCK_GPIO_Port, SPI1_SCK_Pin);
        HAL_GPIO_DeInit(SPI1_MISO_GPIO_Port, SPI1_MISO_Pin);
        HAL_GPIO_DeInit(SPI1_MOSI_GPIO_Port, SPI1_MOSI_Pin);

        // DMA
        HAL_DMA_DeInit(hspi->hdmarx);
        HAL_DMA_DeInit(hspi->hdmatx);

        // Interrupts
        HAL_NVIC_DisableIRQ(SPI1_IRQn);

    }

}

// CRYP MSP Initialization
// This function configures the hardware resources used in this example
void HAL_CRYP_MspInit(CRYP_HandleTypeDef* hcryp)
{

    if (hcryp->Instance==AES) {

        // Peripheral clock enable
        __HAL_RCC_AES_CLK_ENABLE();

        // AES interrupt Init
        HAL_NVIC_SetPriority(AES_IRQn, 0, 0);
        HAL_NVIC_EnableIRQ(AES_IRQn);
    }

}

// CRYP MSP De-Initialization
// This function freeze the hardware resources used in this example
void HAL_CRYP_MspDeInit(CRYP_HandleTypeDef* hcryp)
{

    if (hcryp->Instance==AES) {

        // Peripheral clock disable
        __HAL_RCC_AES_CLK_DISABLE();

        // AES interrupt DeInit
        HAL_NVIC_DisableIRQ(AES_IRQn);

    }

}

// RNG MSP Initialization
void HAL_RNG_MspInit(RNG_HandleTypeDef* hrng)
{
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

    if (hrng->Instance==RNG) {

        // Initializes the peripherals clocks
        PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_RNG;
        PeriphClkInitStruct.RngClockSelection = RCC_RNGCLKSOURCE_LSI;
        if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) {
            Error_Handler();
        }

        // Peripheral clock enable
        __HAL_RCC_RNG_CLK_ENABLE();

        // RNG interrupt Init
        HAL_NVIC_SetPriority(RNG_IRQn, 0, 0);
        HAL_NVIC_EnableIRQ(RNG_IRQn);

    }

}

// RNG MSP De-Initialization
void HAL_RNG_MspDeInit(RNG_HandleTypeDef* hrng)
{

    if (hrng->Instance==RNG) {

        // Peripheral clock disable
        __HAL_RCC_RNG_CLK_DISABLE();

        // RNG interrupt DeInit
        HAL_NVIC_DisableIRQ(RNG_IRQn);

    }

}

// RTC MSP Init
void HAL_RTC_MspInit(RTC_HandleTypeDef* rtcHandle)
{

    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
    if (rtcHandle->Instance==RTC) {

        // Initializes the peripherals clocks
        PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_RTC;
        PeriphClkInitStruct.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;

        if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) {
            Error_Handler();
        }

        // RTC clock enable
        __HAL_RCC_RTC_ENABLE();
        __HAL_RCC_RTCAPB_CLK_ENABLE();

        // RTC interrupt Init
        HAL_NVIC_SetPriority(TAMP_STAMP_LSECSS_SSRU_IRQn, 0, 0);
        HAL_NVIC_EnableIRQ(TAMP_STAMP_LSECSS_SSRU_IRQn);
        HAL_NVIC_SetPriority(RTC_Alarm_IRQn, 0, 0);
        HAL_NVIC_EnableIRQ(RTC_Alarm_IRQn);

    }

}

// RTC MSP DeInit
void HAL_RTC_MspDeInit(RTC_HandleTypeDef* rtcHandle)
{

    if (rtcHandle->Instance==RTC) {

        // Peripheral clock disable
        __HAL_RCC_RTC_DISABLE();
        __HAL_RCC_RTCAPB_CLK_DISABLE();

        // RTC interrupt Deinit
        HAL_NVIC_DisableIRQ(TAMP_STAMP_LSECSS_SSRU_IRQn);
        HAL_NVIC_DisableIRQ(RTC_Alarm_IRQn);

    }

}

// SUBGHZ MSP Init
void HAL_SUBGHZ_MspInit(SUBGHZ_HandleTypeDef* subghzHandle)
{

    // SUBGHZ clock enable
    __HAL_RCC_SUBGHZSPI_CLK_ENABLE();

    // SUBGHZ interrupt Init
    HAL_NVIC_SetPriority(SUBGHZ_Radio_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(SUBGHZ_Radio_IRQn);

}

// SUBGHZ MSP DeInit
void HAL_SUBGHZ_MspDeInit(SUBGHZ_HandleTypeDef* subghzHandle)
{

    // Peripheral clock disable
    __HAL_RCC_SUBGHZSPI_CLK_DISABLE();

    // SUBGHZ interrupt Deinit
    HAL_NVIC_DisableIRQ(SUBGHZ_Radio_IRQn);

}

// UART MSP Initialization
void HAL_UART_MspInit(UART_HandleTypeDef* uartHandle)
{

    GPIO_InitTypeDef GPIO_InitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

    if (uartHandle->Instance==USART1) {

        // Initializes the peripherals clocks
        PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USART1;
        PeriphClkInitStruct.Usart1ClockSelection = RCC_USART1CLKSOURCE_SYSCLK;
        if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) {
            Error_Handler();
        }

        // USART1 clock enable
        __HAL_RCC_USART1_CLK_ENABLE();

        // USART1 GPIO Configuration
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = USART1_GPIO_AF;
        GPIO_InitStruct.Pin = USART1_RX_Pin;
        HAL_GPIO_Init(USART1_RX_GPIO_Port, &GPIO_InitStruct);
        GPIO_InitStruct.Pin = USART1_TX_Pin;
        HAL_GPIO_Init(USART1_TX_GPIO_Port, &GPIO_InitStruct);

        // USART1 DMA Init
        hdma_usart1_tx.Instance = USART1_TX_DMA_Channel;
        hdma_usart1_tx.Init.Request = DMA_REQUEST_USART1_TX;
        hdma_usart1_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
        hdma_usart1_tx.Init.PeriphInc = DMA_PINC_DISABLE;
        hdma_usart1_tx.Init.MemInc = DMA_MINC_ENABLE;
        hdma_usart1_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hdma_usart1_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
        hdma_usart1_tx.Init.Mode = DMA_NORMAL;
        hdma_usart1_tx.Init.Priority = DMA_PRIORITY_LOW;
        if (HAL_DMA_Init(&hdma_usart1_tx) != HAL_OK) {
            Error_Handler();
        }
        if (HAL_DMA_ConfigChannelAttributes(&hdma_usart1_tx, DMA_CHANNEL_NPRIV) != HAL_OK) {
            Error_Handler();
        }
        __HAL_LINKDMA(uartHandle,hdmatx,hdma_usart1_tx);

        // USART1_RX Init
        hdma_usart1_rx.Instance = USART1_RX_DMA_Channel;;
        hdma_usart1_rx.Init.Request = DMA_REQUEST_USART1_RX;
        hdma_usart1_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
        hdma_usart1_rx.Init.PeriphInc = DMA_PINC_DISABLE;
        hdma_usart1_rx.Init.MemInc = DMA_MINC_ENABLE;
        hdma_usart1_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hdma_usart1_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
        hdma_usart1_rx.Init.Mode = DMA_NORMAL;
        hdma_usart1_rx.Init.Priority = DMA_PRIORITY_LOW;
        if (HAL_DMA_Init(&hdma_usart1_rx) != HAL_OK) {
            Error_Handler();
        }
        __HAL_LINKDMA(uartHandle,hdmarx,hdma_usart1_rx);

        // USART1 interrupt Init
        HAL_NVIC_SetPriority(USART1_IRQn, 2, 0);
        HAL_NVIC_EnableIRQ(USART1_IRQn);

    }

    if (uartHandle->Instance==USART2) {

        // Initializes the peripherals clocks
        PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USART2;
        PeriphClkInitStruct.Usart2ClockSelection = RCC_USART2CLKSOURCE_SYSCLK;
        if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) {
            Error_Handler();
        }

        // USART2 clock enable
        __HAL_RCC_USART2_CLK_ENABLE();

        // USART2 GPIO Configuration
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = USART2_GPIO_AF;
        GPIO_InitStruct.Pin = USART2_RX_Pin;
        HAL_GPIO_Init(USART2_RX_GPIO_Port, &GPIO_InitStruct);
        GPIO_InitStruct.Pin = USART2_TX_Pin;
        HAL_GPIO_Init(USART2_TX_GPIO_Port, &GPIO_InitStruct);

        // USART2 DMA Init
        hdma_usart2_tx.Instance = USART2_TX_DMA_Channel;
        hdma_usart2_tx.Init.Request = DMA_REQUEST_USART2_TX;
        hdma_usart2_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
        hdma_usart2_tx.Init.PeriphInc = DMA_PINC_DISABLE;
        hdma_usart2_tx.Init.MemInc = DMA_MINC_ENABLE;
        hdma_usart2_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hdma_usart2_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
        hdma_usart2_tx.Init.Mode = DMA_NORMAL;
        hdma_usart2_tx.Init.Priority = DMA_PRIORITY_LOW;
        if (HAL_DMA_Init(&hdma_usart2_tx) != HAL_OK) {
            Error_Handler();
        }
        if (HAL_DMA_ConfigChannelAttributes(&hdma_usart2_tx, DMA_CHANNEL_NPRIV) != HAL_OK) {
            Error_Handler();
        }
        __HAL_LINKDMA(uartHandle,hdmatx,hdma_usart2_tx);

        // USART2_RX Init
#ifdef USE_USART2_RX_DMA
        hdma_usart2_rx.Instance = USART2_RX_DMA_Channel;;
        hdma_usart2_rx.Init.Request = DMA_REQUEST_USART2_RX;
        hdma_usart2_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
        hdma_usart2_rx.Init.PeriphInc = DMA_PINC_DISABLE;
        hdma_usart2_rx.Init.MemInc = DMA_MINC_ENABLE;
        hdma_usart2_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hdma_usart2_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
        hdma_usart2_rx.Init.Mode = DMA_NORMAL;
        hdma_usart2_rx.Init.Priority = DMA_PRIORITY_LOW;
        if (HAL_DMA_Init(&hdma_usart2_rx) != HAL_OK) {
            Error_Handler();
        }
        __HAL_LINKDMA(uartHandle,hdmarx,hdma_usart2_rx);
#endif

        // USART2 interrupt Init
        HAL_NVIC_SetPriority(USART2_IRQn, 2, 0);
        HAL_NVIC_EnableIRQ(USART2_IRQn);

    }

    if (uartHandle->Instance==LPUART1) {

        // Initializes the peripherals clocks
        PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_LPUART1;
        PeriphClkInitStruct.Lpuart1ClockSelection = RCC_LPUART1CLKSOURCE_LSE;
        if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) {
            Error_Handler();
        }

        // Peripheral clock enable
        __HAL_RCC_LPUART1_CLK_ENABLE();

        // LPUART1 GPIO Configuration
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        GPIO_InitStruct.Alternate = LPUART1_GPIO_AF;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pin = LPUART1_TX_Pin;
        HAL_GPIO_Init(LPUART1_TX_GPIO_Port, &GPIO_InitStruct);
        GPIO_InitStruct.Pin = LPUART1_RX_Pin;
        HAL_GPIO_Init(LPUART1_RX_GPIO_Port, &GPIO_InitStruct);

        // LPUART1 interrupt Init
        HAL_NVIC_SetPriority(LPUART1_IRQn, 2, 0);
        HAL_NVIC_EnableIRQ(LPUART1_IRQn);

    }

}

// UART MSP Deinitialization
void HAL_UART_MspDeInit(UART_HandleTypeDef* uartHandle)
{

    if (uartHandle->Instance==USART1) {

        // Peripheral clock disable
        __HAL_RCC_USART1_CLK_DISABLE();

        // USART1 DMA DeInit
        HAL_DMA_DeInit(uartHandle->hdmatx);
        HAL_DMA_DeInit(uartHandle->hdmarx);

        // USART1 GPIO Configuration
        HAL_GPIO_DeInit(USART1_TX_GPIO_Port, USART1_TX_Pin);
        HAL_GPIO_DeInit(USART1_RX_GPIO_Port, USART1_RX_Pin);

        // USART1 interrupt Deinit
        HAL_NVIC_DisableIRQ(USART1_IRQn);

    }

    if (uartHandle->Instance==USART2) {

        // Peripheral clock disable
        __HAL_RCC_USART2_CLK_DISABLE();

        // USART2 DMA DeInit
        HAL_DMA_DeInit(uartHandle->hdmatx);
#ifdef USE_USART2_RX_DMA
        HAL_DMA_DeInit(uartHandle->hdmarx);
#endif

        // USART2 GPIO Configuration
        HAL_GPIO_DeInit(USART2_TX_GPIO_Port, USART2_TX_Pin);
        HAL_GPIO_DeInit(USART2_RX_GPIO_Port, USART2_RX_Pin);

        // USART2 interrupt Deinit
        HAL_NVIC_DisableIRQ(USART2_IRQn);

    }

    if (uartHandle->Instance==LPUART1) {

        // Peripheral clock disable
        __HAL_RCC_LPUART1_CLK_DISABLE();

        // LPUART1 GPIO Configuration
        HAL_GPIO_DeInit(LPUART1_TX_GPIO_Port, LPUART1_TX_Pin);
        HAL_GPIO_DeInit(LPUART1_RX_GPIO_Port, LPUART1_RX_Pin);

        // LPUART1 interrupt DeInit
        HAL_NVIC_DisableIRQ(LPUART1_IRQn);

    }

}
