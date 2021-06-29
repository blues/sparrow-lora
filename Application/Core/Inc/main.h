// Copyright 2021 Blues Inc.  All rights reserved.
// Use of this source code is governed by licenses granted by the
// copyright holder including that found in the LICENSE file.

#pragma once

#include <stdbool.h>
#include <string.h>
#include "stm32wlxx_hal.h"
#include "config_sys.h"
#include "board.h"

void MX_GPIO_Init(void);
void MX_DMA_Init(void);
void MX_ADC_Init(void);
void MX_ADC_DeInit(void);
bool MX_ADC_Values(uint16_t *wordValues, double *voltageValues, double *vref);
double MX_ADC_A0_Voltage(void);
void MX_USART1_UART_Init(void);
void MX_USART1_UART_Transmit(uint8_t *buf, uint32_t len);
void MX_USART1_UART_DeInit(void);
void MX_USART2_UART_Init(void);
void MX_USART2_UART_Suspend(void);
void MX_USART2_UART_Resume(void);
void MX_USART2_UART_Transmit(uint8_t *buf, uint32_t len);
void MX_USART2_UART_DeInit(void);
void MX_LPUART1_UART_Init(void);
void MX_LPUART1_UART_Suspend(void);
void MX_LPUART1_UART_Resume(void);
void MX_LPUART1_UART_Transmit(uint8_t *buf, uint32_t len);
void MX_LPUART1_UART_DeInit(void);
void MX_I2C2_Init(void);
void MX_I2C2_DeInit(void);
bool MY_I2C2_ReadRegister(uint16_t i2cAddress, uint8_t Reg, void *data, uint16_t maxdatalen, uint32_t timeoutMs);
bool MY_I2C2_WriteRegister(uint16_t i2cAddress, uint8_t Reg, void *data, uint16_t datalen, uint32_t timeoutMs);
bool MY_I2C2_Transmit(uint16_t i2cAddress, void *data, uint16_t datalen, uint32_t timeoutMs);
bool MY_I2C2_Receive(uint16_t i2cAddress, void *data, uint16_t maxdatalen, uint32_t timeoutMs);
void MX_SPI1_Init(void);
void MX_SPI1_DeInit(void);
void MX_SUBGHZ_Init(void);
void MX_RNG_Init(void);
void MX_RNG_DeInit(void);
void MX_AES_Init(void);
void MX_AES_DeInit(void);
void MX_RTC_Init(void);
uint32_t HAL_GetTickMs(void);
void MX_DBG_Init(void);
void MX_DBG_Suspend(void);
void MX_DBG_Resume(void);
void MX_DBG(const char *msg, size_t len);
void MX_DBG_Disable(void);
void MX_DBG_Enable(void);
bool MX_DBG_Active(void);
bool MX_DBG_Available(void);
uint8_t MX_DBG_Receive(bool *underrun, bool *overrun);
void MX_TIM17_DelayUs(uint32_t us);
#define HAL_DelayUs(us) MX_TIM17_DelayUs(us)

uint32_t MX_Image_Size(void);
#define MX_Image_Pages() (((MX_Image_Size()%FLASH_PAGE_SIZE)==0)?(MX_Image_Size()/FLASH_PAGE_SIZE):((MX_Image_Size()/FLASH_PAGE_SIZE)+1))
uint32_t MX_Heap_Size(uint8_t **base);

void MX_Breakpoint(void);
void MX_AppMain(void);
void MX_UTIL_Init(void);
void MX_AppISR(uint16_t GPIO_Pin);
uint32_t MX_RNG_Get(void);
bool MX_AES_CTR_Encrypt(uint8_t *key, uint8_t *plaintext, uint16_t len, uint8_t *ciphertext);
bool MX_AES_CTR_Decrypt(uint8_t *key, uint8_t *ciphertext, uint16_t len, uint8_t *plaintext);

void Error_Handler(void);

extern RTC_HandleTypeDef hrtc;
extern SUBGHZ_HandleTypeDef hsubghz;
extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef hlpuart1;
extern DMA_HandleTypeDef hdma_usart2_tx;
