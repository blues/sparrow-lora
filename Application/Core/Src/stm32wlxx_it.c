// Copyright 2022 Blues Inc.  All rights reserved.
// Use of this source code is governed by licenses granted by the
// copyright holder including that found in the LICENSE file.

#include "main.h"
#include "stm32wlxx_it.h"
#include "stm32wlxx_hal_gpio.h"
#include "stm32wlxx_hal_cryp.h"
#include "stm32wlxx_hal_rng.h"

extern DMA_HandleTypeDef hdma_adc;
extern ADC_HandleTypeDef hadc;
extern DMA_HandleTypeDef hdma_i2c2_rx;
extern DMA_HandleTypeDef hdma_i2c2_tx;
extern I2C_HandleTypeDef hi2c2;
extern DMA_HandleTypeDef hdma_spi1_rx;
extern DMA_HandleTypeDef hdma_spi1_tx;
extern SPI_HandleTypeDef hspi1;
extern SUBGHZ_HandleTypeDef hsubghz;
extern DMA_HandleTypeDef hdma_usart1_rx;
extern DMA_HandleTypeDef hdma_usart1_tx;
extern DMA_HandleTypeDef hdma_usart2_rx;
extern DMA_HandleTypeDef hdma_usart2_tx;
extern UART_HandleTypeDef hlpuart1;
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
extern RTC_HandleTypeDef hrtc;
extern CRYP_HandleTypeDef hcryp;
extern RNG_HandleTypeDef hrng;

// Forwards
void EXTI0_IRQHandler(void);
void EXTI1_IRQHandler(void);
void EXTI2_IRQHandler(void);
void EXTI3_IRQHandler(void);
void EXTI4_IRQHandler(void);
void EXTI9_5_IRQHandler(void);
void EXTI15_10_IRQHandler(void);
void GPIO_EXTI_IRQHandler(uint16_t GPIO_Pin);
void AES_IRQHandler(void);
void RNG_IRQHandler(void);

// For panic breakpoint
void MX_Breakpoint()
{
    if (MX_DBG_Active()) {
        asm ("BKPT 0");
    }
}

// Non-Maskable Interrupt
void NMI_Handler(void)
{
    MX_Breakpoint();
    NVIC_SystemReset();
}

// Hardfault Interrupt
void HardFault_Handler(void)
{
    MX_Breakpoint();
    NVIC_SystemReset();
}

// Memory management fault
void MemManage_Handler(void)
{
    MX_Breakpoint();
    NVIC_SystemReset();
}

// Prefetch fault and memory access fault.
void BusFault_Handler(void)
{
    MX_Breakpoint();
    NVIC_SystemReset();
}

// Undefined instruction or illegal state.
void UsageFault_Handler(void)
{
    MX_Breakpoint();
}

// This function handles System service call via SWI instruction.
void SVC_Handler(void)
{
}

// Debug monitor
void DebugMon_Handler(void)
{
}

// Pendable request for system service.
void PendSV_Handler(void)
{
}

// System tick timer
void SysTick_Handler(void)
{
    HAL_IncTick();
}

// RTC Tamper, RTC TimeStamp, LSECSS and RTC SSRU Interrupts
void TAMP_STAMP_LSECSS_SSRU_IRQHandler(void)
{
    HAL_RTCEx_SSRUIRQHandler(&hrtc);
}

// DMA Handlers
void SPI1_RX_DMA_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_spi1_rx);
}
void SPI1_TX_DMA_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_spi1_tx);
}
void I2C2_RX_DMA_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_i2c2_rx);
}
void I2C2_TX_DMA_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_i2c2_tx);
}
void ADC_DMA_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_adc);
}
void USART1_RX_DMA_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_usart1_rx);
}
void USART1_TX_DMA_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_usart1_tx);
}
#ifdef USE_USART2_RX_DMA
void USART2_RX_DMA_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_usart2_rx);
}
#endif
void USART2_TX_DMA_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_usart2_tx);
}

// ADC Interrupt
void ADC_IRQHandler(void)
{
    HAL_ADC_IRQHandler(&hadc);
}

// I2C2 Event Interrupt
void I2C2_EV_IRQHandler(void)
{
    HAL_I2C_EV_IRQHandler(&hi2c2);
}

// I2C2 Error Interrupt
void I2C2_ER_IRQHandler(void)
{
    HAL_I2C_ER_IRQHandler(&hi2c2);
}

// SPI1 Interrupt
void SPI1_IRQHandler(void)
{
    HAL_SPI_IRQHandler(&hspi1);
}

// USART1 Interrupt
void USART1_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart1);
}

// USART2 Interrupt
void USART2_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart2);
}

// LPUART1 Interrupt
void LPUART1_IRQHandler(void)
{
    HAL_UART_IRQHandler(&hlpuart1);
}

// RTC Alarms (A and B) Interrupt.
void RTC_Alarm_IRQHandler(void)
{
    HAL_RTC_AlarmIRQHandler(&hrtc);
}

// AES Interrupt
void AES_IRQHandler(void)
{
    HAL_CRYP_IRQHandler(&hcryp);
}

// RNG Interrupt
void RNG_IRQHandler(void)
{
    HAL_RNG_IRQHandler(&hrng);
}

// SUBGHZ Radio Interrupt
void SUBGHZ_Radio_IRQHandler(void)
{
    HAL_SUBGHZ_IRQHandler(&hsubghz);
}

// Method called when an external interrupt happens
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    MX_AppISR(GPIO_Pin);
}

void GPIO_EXTI_IRQHandler(uint16_t GPIO_Pin)
{
    if(__HAL_GPIO_EXTI_GET_IT(GPIO_Pin) != RESET) {
        uint16_t GPIO_Line = GPIO_Pin & EXTI->PR1;
        __HAL_GPIO_EXTI_CLEAR_IT(GPIO_Pin);
        HAL_GPIO_EXTI_Callback(GPIO_Line);
    }
}
void EXTI0_IRQHandler( void )
{
    GPIO_EXTI_IRQHandler(GPIO_PIN_0);
}
void EXTI1_IRQHandler( void )
{
    GPIO_EXTI_IRQHandler(GPIO_PIN_1);
}
void EXTI2_IRQHandler( void )
{
    GPIO_EXTI_IRQHandler(GPIO_PIN_2);
}
void EXTI3_IRQHandler( void )
{
    GPIO_EXTI_IRQHandler(GPIO_PIN_3);
}
void EXTI4_IRQHandler( void )
{
    GPIO_EXTI_IRQHandler(GPIO_PIN_4);
}
void EXTI9_5_IRQHandler( void )
{
    GPIO_EXTI_IRQHandler(GPIO_PIN_9|GPIO_PIN_8|GPIO_PIN_7|GPIO_PIN_6|GPIO_PIN_5);
}
void EXTI15_10_IRQHandler( void )
{
    GPIO_EXTI_IRQHandler(GPIO_PIN_15|GPIO_PIN_14|GPIO_PIN_13|GPIO_PIN_12|GPIO_PIN_11|GPIO_PIN_10);
}

// This function is executed in case of error occurrence.
void Error_Handler(void)
{
    MX_Breakpoint();
    NVIC_SystemReset();
}

// Reports the name of the source file and the source line number
// where the assert_param error has occurred.
#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    MX_Breakpoint();
    NVIC_SystemReset();
}
#endif
