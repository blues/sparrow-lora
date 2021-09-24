// Copyright 2021 Blues Inc.  All rights reserved.
// Use of this source code is governed by licenses granted by the
// copyright holder including that found in the LICENSE file.

// Board/pin definitions (also see board_radio.h)
#pragma once

#include "config_sys.h"
#include "stm32wlxx_hal.h"
#include "stm32wlxx_hal_exti.h"
#include "board_radio.h"

#define BOARD_NUCLEO    0           // NUCLEO-WL55JC1
#define BOARD_V1        1
#define CURRENT_BOARD   BOARD_V1

// All pins on the STM32WLE5 UFQFPN48 package
#define PB3     1                   // RFSEL_0
#define PB4     2                   // A0
#define PB5     3                   // RFSEL_1
#define PB6     4                   // USART1_TX (D1)
#define PB7     5                   // USART1_RX (D0)
#define PB8     6                   // FE_CTRL3
#define PA0     7                   // LED_RED
#define PA1     8                   // LED_BLUE
#define PA2     9                   // LPUART1_TX
#define PA3     10                  // LPUART1_RX
#define PA4     12                  // SPI1_CS (D10)
#define PA5     13                  // SPI1_SCK (D13)
#define PA6     14                  // SPI1_MISO (D12)
#define PA7     15                  // SPI1_MOSI (D11)
#define PA8     16                  // FE_CTRL1
#define PA9     17                  // FE_CTRL2
#define PB2     31                  // A1
#define PB12    32                  // LED_GREEN
#define PA10    33                  // A2
#define PA11    34                  // I2C2_SDA (D14)
#define PA12    35                  // I2C2_SCL (D15)
#define PA13    36                  // SWDIO
#define PC13    38                  // BUTTON1
#define PA14    42                  // SWCLK
#define PA15    43                  // A3

// Arduino pin definitions on the Nucleo
#define ARDUINO_TL_1                // XX NC
#define ARDUINO_TL_2                // IOREF (5V OUT on Uno)
#define ARDUINO_TL_3                // Reset (STM32 T_NRST)
#define ARDUINO_TL_4                // 3V3_VOUT (3V3 input/output)
#define ARDUINO_TL_5                // 5V_VOUT
#define ARDUINO_TL_6                // GND
#define ARDUINO_TL_7                // GND
#define ARDUINO_TL_8                // VIN 7-12V
#define ARDUINO_BL_1                // XX A0 (STM32 PB1, ADC1_IN5)
#define ARDUINO_BL_2                // A1 (STM32 PB2, ADC1_IN4)
#define ARDUINO_BL_3                // XX A2 (STM32 PA10, ADC1_IN6)
#define ARDUINO_BL_4                // XX A3 (STM32 PB4, ADC_IN3)
#define ARDUINO_BL_5                // XX A4 (STM32 PB14, ADC1_IN1, I2C_SDA)
#define ARDUINO_BL_6                // XX A5 (STM32 PB13, ADC1_IN0, I2C3_SCL)
#define ARDUINO_TR_10               // SCL/D15 (STM32 PA12)
#define ARDUINO_TR_9                // SDA/D14 (STM32 PA11)
#define ARDUINO_TR_8                // XX AVDD
#define ARDUINO_TR_7                // GND
#define ARDUINO_TR_6                // SCK/D13 (STM32 PA5)
#define ARDUINO_TR_5                // MISO/D12 (STM32 PA6)
#define ARDUINO_TR_4                // MOSI/D11 (STM32 PA7)
#define ARDUINO_TR_3                // CS/D10 (STM32 PA4)
#define ARDUINO_TR_2                // XX D9 (STM32 PA9)
#define ARDUINO_TR_1                // XX D8 (STM32 PC2)
#define ARDUINO_BR_8                // XX D7 (STM32 PC1)
#define ARDUINO_BR_7                // XX D6 (STM32 PB10)
#define ARDUINO_BR_6                // XX D5 (STM32 PB8)
#define ARDUINO_BR_5                // XX D4 (STM32 PB5)
#define ARDUINO_BR_4                // XX D3 (STM32 PB3)
#define ARDUINO_BR_3                // XX D2 (STM32 PB12)
#define ARDUINO_BR_2                // TX/D1 (STM32 ARD USART1 PB6 TX)
#define ARDUINO_BR_1                // RX/D0 (STM32 ARD USART1 PB7 RX)

// Primary SPI.  Note that the use of SPI1 on these pins prevents the use of
// the DEBUG_SUBGHZSPI function, which only exists on these pins.
#define SPI1_CS_Pin                     GPIO_PIN_4          // PA4
#define SPI1_CS_GPIO_Port               GPIOA
#define SPI1_SCK_Pin                    GPIO_PIN_5          // PA5
#define SPI1_SCK_GPIO_Port              GPIOA
#define SPI1_MISO_Pin                   GPIO_PIN_6          // PA6
#define SPI1_MISO_GPIO_Port             GPIOA
#define SPI1_MOSI_Pin                   GPIO_PIN_7          // PA7
#define SPI1_MOSI_GPIO_Port             GPIOA
#define SPI1_GPIO_AF                    GPIO_AF5_SPI1
#define SPI1_RX_DMA_Channel             DMA2_Channel1
#define SPI1_RX_DMA_IRQn                DMA2_Channel1_IRQn
#define SPI1_RX_DMA_IRQHandler          DMA2_Channel1_IRQHandler
#define SPI1_TX_DMA_Channel             DMA2_Channel2
#define SPI1_TX_DMA_IRQn                DMA2_Channel2_IRQn
#define SPI1_TX_DMA_IRQHandler          DMA2_Channel2_IRQHandler

// I2C2 --  Note that on the UFQFPN48 package, SCL is only available on PA12.
// This prevents any possible use of RF_BUSY, which is ONLY available on PA12.
#define I2C2_SDA_Pin                    GPIO_PIN_11         // PA11
#define I2C2_SDA_GPIO_Port              GPIOA
#define I2C2_SDA_GPIO_AF                GPIO_AF4_I2C2
#define I2C2_SCL_Pin                    GPIO_PIN_12         // PA12
#define I2C2_SCL_GPIO_Port              GPIOA
#define I2C2_SCL_GPIO_AF                GPIO_AF4_I2C2
#define I2C2_RX_DMA_Channel             DMA1_Channel5
#define I2C2_RX_DMA_IRQn                DMA1_Channel5_IRQn
#define I2C2_RX_DMA_IRQHandler          DMA1_Channel5_IRQHandler
#define I2C2_TX_DMA_Channel             DMA1_Channel6
#define I2C2_TX_DMA_IRQn                DMA1_Channel6_IRQn
#define I2C2_TX_DMA_IRQHandler          DMA1_Channel6_IRQHandler

// ADC
#define ADC_DMA_Channel                 DMA1_Channel7
#define ADC_DMA_IRQn                    DMA1_Channel7_IRQn
#define ADC_DMA_IRQHandler              DMA1_Channel7_IRQHandler
#define VREFINT_ADC_Channel             ADC_CHANNEL_VREFINT
#define VREFINT_ADC_RankIndex           0                   // VREFINT will always be first
#define VREFINT_ADC_Rank                ADC_REGULAR_RANK_1
#define A0_Pin                          GPIO_PIN_4      	// PB4 (A0 is used for battery monitoring)
#define A0_GPIO_Port                    GPIOB
#define A0_ADC_Channel                  ADC_CHANNEL_3
#define A0_ADC_RankIndex                1
#define A0_ADC_Rank                     ADC_REGULAR_RANK_2
#define A1_Pin                          GPIO_PIN_2			// PB2
#define A1_GPIO_Port                    GPIOB
#define A1_ADC_Channel                  ADC_CHANNEL_4
#define A1_ADC_RankIndex                2
#define A1_ADC_Rank                     ADC_REGULAR_RANK_3
#define A2_Pin                          GPIO_PIN_10         // PA10
#define A2_GPIO_Port                    GPIOA
#define A2_ADC_Channel                  ADC_CHANNEL_6
#define A2_ADC_RankIndex                3
#define A2_ADC_Rank                     ADC_REGULAR_RANK_4
#define A3_Pin                          GPIO_PIN_15         // PA15
#define A3_GPIO_Port                    GPIOA
#define A3_ADC_Channel                  ADC_CHANNEL_11
#define A3_ADC_RankIndex                4
#define A3_ADC_Rank                     ADC_REGULAR_RANK_5
#define ADC_TOTAL                       5                   // # of ADCs
#define ADC_COUNT                       (ADC_TOTAL-1)       // # of usable ADCs without VREFINT

// USART1
#define USART1_BAUDRATE                 115200
#define USART1_TX_Pin                   GPIO_PIN_6          // PB6
#define USART1_TX_GPIO_Port             GPIOB
#define USART1_RX_Pin                   GPIO_PIN_7          // PB7
#define USART1_RX_GPIO_Port             GPIOB
#define USART1_GPIO_AF                  GPIO_AF7_USART1
#define USART1_RX_DMA_Channel           DMA1_Channel1
#define USART1_RX_DMA_IRQn              DMA1_Channel1_IRQn
#define USART1_RX_DMA_IRQHandler        DMA1_Channel1_IRQHandler
#define USART1_TX_DMA_Channel           DMA1_Channel2
#define USART1_TX_DMA_IRQn              DMA1_Channel2_IRQn
#define USART1_TX_DMA_IRQHandler        DMA1_Channel2_IRQHandler

// USART2 (Note that DS13293 shows USART2 (AF7) and LPUART1 (AF8)
// can be used on these same pins.
#define USART2_BAUDRATE                 115200
#define USART2_TX_Pin                   GPIO_PIN_2          // PA2
#define USART2_TX_GPIO_Port             GPIOA
#define USART2_RX_Pin                   GPIO_PIN_3          // PA3
#define USART2_RX_GPIO_Port             GPIOA
#define USART2_GPIO_AF                  GPIO_AF7_USART2
#ifdef USE_USART2_RX_DMA
#define USART2_RX_DMA_Channel           DMA1_Channel3
#define USART2_RX_DMA_IRQn              DMA1_Channel3_IRQn
#define USART2_RX_DMA_IRQHandler        DMA1_Channel3_IRQHandler
#endif
#define USART2_TX_DMA_Channel           DMA1_Channel4
#define USART2_TX_DMA_IRQn              DMA1_Channel4_IRQn
#define USART2_TX_DMA_IRQHandler        DMA1_Channel4_IRQHandler

// LPUART1
#define LPUART1_BAUDRATE                9600
#define LPUART1_TX_Pin                  GPIO_PIN_2          // PA2
#define LPUART1_TX_GPIO_Port            GPIOA
#define LPUART1_RX_Pin                  GPIO_PIN_3          // PA3
#define LPUART1_RX_GPIO_Port            GPIOA
#define LPUART1_GPIO_AF                 GPIO_AF8_LPUART1

// Radio Frequency selector (tri-state detection of 9 states)
#define RFSEL_0_Pin                     GPIO_PIN_3          // PB3
#define RFSEL_0_GPIO_Port               GPIOB
#define RFSEL_1_Pin                     GPIO_PIN_5          // PB5
#define RFSEL_1_GPIO_Port               GPIOB

// Excelitas PYQ 1548/7660 Motion Detector
#define PYQ_SERIAL_IN_Pin               GPIO_PIN_6          // PA6
#define PYQ_SERIAL_IN_Port              GPIOA
#define PYQ_DIRECT_LINK_Pin             GPIO_PIN_7          // PA7
#define PYQ_DIRECT_LINK_Port            GPIOA
#define PYQ_DIRECT_LINK_EXTI_IRQn       EXTI9_5_IRQn
#define PYQ_DIRECT_LINK_IT_PRIORITY     15

// BME Power Pin - powering peripherals on a switched i2c bus
#define BME_POWER_Pin                   GPIO_PIN_5          // PA5
#define BME_POWER_GPIO_Port             GPIOA

// LEDs
#if (CURRENT_BOARD != BOARD_NUCLEO)
#define LED_BLUE_Pin                    GPIO_PIN_1          // PA1
#define LED_BLUE_GPIO_Port              GPIOA
#define LED_GREEN_Pin                   GPIO_PIN_12         // PB12
#define LED_GREEN_GPIO_Port             GPIOB
#define LED_RED_Pin                     GPIO_PIN_0          // PA0
#define LED_RED_GPIO_Port               GPIOA
#else
#define LED_BLUE_Pin                    GPIO_PIN_15
#define LED_BLUE_GPIO_Port              GPIOB
#define LED_GREEN_Pin                   GPIO_PIN_9
#define LED_GREEN_GPIO_Port             GPIOB
#define LED_RED_Pin                     GPIO_PIN_11
#define LED_RED_GPIO_Port               GPIOB
#endif

#if (CURRENT_BOARD != BOARD_NUCLEO)
#define BUTTON1_ACTIVE_HIGH             false
#define BUTTON1_Pin                     GPIO_PIN_13         // PC13
#define BUTTON1_GPIO_Port               GPIOC
#define BUTTON1_EXTI_IRQn               EXTI15_10_IRQn
#else
#define BUTTON1_ACTIVE_HIGH             false
#define BUTTON1_Pin                     GPIO_PIN_0          // PA0
#define BUTTON1_GPIO_Port               GPIOA
#define BUTTON1_EXTI_IRQn               EXTI0_IRQn
#endif

#define BUTTONx_IT_PRIORITY             15

// Radio debugging
#if (DEBUGGER_ON && DEBUGGER_RADIO_DBG_GPIO)
#define DBG_LINE1_Pin                   GPIO_PIN_?
#define DBG_LINE1_GPIO_Port             GPIO?
#define DBG_LINE2_Pin                   GPIO_PIN_?
#define DBG_LINE2_GPIO_Port              GPIO?
#define DBG_LINE3_Pin                   GPIO_PIN_?
#define DBG_LINE3_GPIO_Port             GPIO?
#define DBG_LINE4_Pin                   GPIO_PIN_?
#define DBG_LINE4_GPIO_Port             GPIO?
#define DBG_GPIO_WRITE( gpio, n, x )    HAL_GPIO_WritePin( gpio, n, (GPIO_PinState)(x) )
#define DBG_GPIO_SET_LINE( gpio, n )    LL_GPIO_SetOutputPin( gpio, n )
#define DBG_GPIO_RST_LINE( gpio, n )    LL_GPIO_ResetOutputPin ( gpio, n )
#else
#define DBG_GPIO_SET_LINE( gpio, n )
#define DBG_GPIO_RST_LINE( gpio, n )
#endif // Radio GPIO debugging

// Radio front-end control, whose pins are selected based on board type
#if (CURRENT_BOARD != BOARD_NUCLEO)
#define FE_CTRL1_Pin                 GPIO_PIN_8             // PA8
#define FE_CTRL1_GPIO_Port           GPIOA
#define FE_CTRL2_Pin                 GPIO_PIN_9             // PA9
#define FE_CTRL2_GPIO_Port           GPIOA
#define FE_CTRL3_Pin                 GPIO_PIN_8             // PB8
#define FE_CTRL3_GPIO_Port           GPIOB
#else
#define FE_CTRL1_Pin                 GPIO_PIN_4
#define FE_CTRL1_GPIO_Port           GPIOC
#define FE_CTRL2_Pin                 GPIO_PIN_5
#define FE_CTRL2_GPIO_Port           GPIOC
#define FE_CTRL3_Pin                 GPIO_PIN_3
#define FE_CTRL3_GPIO_Port           GPIOC
#endif

// TXCO control
#if IS_TCXO_SUPPORTED
#define RF_TCXO_VCC_Pin                 GPIO_PIN_0          // PB0
#define RF_TCXO_VCC_GPIO_Port           GPIOB
#endif

// Debugging
#define SWDIO_Pin                       GPIO_PIN_13         // PA13
#define SWDIO_GPIO_Port                 GPIOA
#define SWCLK_Pin                       GPIO_PIN_14         // PA14
#define SWCLK_GPIO_Port                 GPIOA

// RTC
#define RTC_N_PREDIV_S          10
#define RTC_PREDIV_S            ((1<<RTC_N_PREDIV_S)-1)
#define RTC_PREDIV_A            ((1<<(15-RTC_N_PREDIV_S))-1)

// Define flash parameters
#if defined(STM32WL55xx) || defined(STM32WL54xx) || defined(STM32WLE5xx) || defined(STM32WLE4xx)
#define FLASH_BASE_ADDRESS      FLASH_BASE
#define FLASH_TOTAL_BYTES       FLASH_SIZE
#endif

// Value of analog reference voltage connected to supply Vdda (mV)
#define VDDA_APPLI          (3300U)
#if (CURRENT_BOARD != BOARD_NUCLEO)
#define BATMON_ADJUSTMENT   3           // Multiplier for this version of battery monitor
#endif

// IO vars
extern uint32_t ioRFFrequency;
