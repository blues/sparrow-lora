// Copyright 2022 Blues Inc.  All rights reserved.
// Use of this source code is governed by licenses granted by the
// copyright holder including that found in the LICENSE file.

#include "main.h"
#include "stm32wlxx_hal_cryp.h"
#include "stm32wlxx_hal_rng.h"
#include "stm32wlxx_ll_lpuart.h"

// HAL data
RNG_HandleTypeDef hrng;
RTC_HandleTypeDef hrtc;
SUBGHZ_HandleTypeDef hsubghz;
ADC_HandleTypeDef hadc;
DMA_HandleTypeDef hdma_adc;
UART_HandleTypeDef hlpuart1;
UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;
DMA_HandleTypeDef hdma_usart1_rx;
DMA_HandleTypeDef hdma_usart1_tx;
DMA_HandleTypeDef hdma_usart2_rx;
DMA_HandleTypeDef hdma_usart2_tx;
CRYP_HandleTypeDef hcryp;
I2C_HandleTypeDef hi2c2;
DMA_HandleTypeDef hdma_i2c2_rx;
DMA_HandleTypeDef hdma_i2c2_tx;
SPI_HandleTypeDef hspi1;
DMA_HandleTypeDef hdma_spi1_rx;
DMA_HandleTypeDef hdma_spi1_tx;
TIM_HandleTypeDef htim17;
uint32_t i2c2IOCompletions = 0;

// ADC buffer
#if defined ( __ICCARM__ ) /* IAR Compiler */
#pragma data_alignment=8
#elif defined ( __CC_ARM ) /* ARM Compiler */
__align(8)
#elif defined ( __GNUC__ ) /* GCC Compiler */
__attribute__ ((aligned (8)))
#endif
uint16_t adcValues[ADC_TOTAL] = {0};
bool adcDMACompleted = false;

// Peripheral mask, so we can easily tell what is enabled and what is not
#define PERIPHERAL_RNG      0x00000001
#define PERIPHERAL_RTC      0x00000002
#define PERIPHERAL_SUBGHZ   0x00000004
#define PERIPHERAL_ADC      0x00000008
#define PERIPHERAL_ADCDMA   0x00000010
#define PERIPHERAL_LPUART1  0x00000020
#define PERIPHERAL_USART1   0x00000040
#define PERIPHERAL_USART2   0x00000080
#define PERIPHERAL_CRYP     0x00000100
#define PERIPHERAL_I2C2     0x00000200
#define PERIPHERAL_SPI1     0x00000400
#define PERIPHERAL_SPI1DMA  0x00000800
#define PERIPHERAL_TIM17    0x00001000
uint32_t peripherals = 0;

// RTC
#define BASEYEAR 2000   // Must end in 00 because of the chip's leap year computations

// AES
__ALIGN_BEGIN static uint32_t keyAES[8] __ALIGN_END = {
    0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
};
__ALIGN_BEGIN static uint32_t AESIV_CTR[4] __ALIGN_END = {0xF0F1F2F3, 0xF4F5F6F7, 0xF8F9FAFB, 0xFCFDFEFF};

// Linker-related symbols
#if defined( __ICCARM__ )   // IAR
extern void *ROM_CONTENT$$Limit;
extern void *HEAP$$Base;
extern void *HEAP$$Limit;
#else                       // STM32CubeIDE (gcc)
extern void *_highest_used_rom;
extern void *_end;
extern void *_estack;
extern uint32_t _Min_Stack_Size;
#endif

// See reference manual RM0453.  The size is the last entry's address in Table 89 + sizeof(uint32_t).
// (Don't be confused into using the entry number rather than the address, because there are negative
// entry numbers. The highest address is the only accurate way of determining the actual table size.)
#if defined(STM32WL55xx) || defined(STM32WLE5xx)
#define VECTOR_TABLE_SIZE_BYTES (0x0134+sizeof(uint32_t))
#else
#error "Please look up NVIC table size for this processor in reference manual."
#endif
#if defined ( __ICCARM__ ) /* IAR Compiler */
#pragma data_alignment=512
#elif defined ( __CC_ARM ) /* ARM Compiler */
__align(512)
#elif defined ( __GNUC__ ) /* GCC Compiler */
__attribute__ ((aligned (512)))
#endif
static uint8_t vector_t[VECTOR_TABLE_SIZE_BYTES];

// Forwards
void SystemClock_Config(void);
static void MX_TIM17_Init(void);
double calibrateVoltage(double v);
size_t strlcat(char *dst, const char *src, size_t siz);

// Main entry point
int main(void)
{

    // Copy the vectors
    memcpy(vector_t, (uint8_t *) FLASH_BASE, sizeof(vector_t));
    SCB->VTOR = (uint32_t) vector_t;

    // Reset of all peripherals, Initializes the Flash interface and the Systick.
    HAL_Init();

    // Configure the system clock
    SystemClock_Config();

    // Initialize for IO
    MX_TIM17_Init();
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_DBG_Init();

    // Initialize ST utilities
    MX_UTIL_Init();

    // Run the app, which will init its own peripherals
    MX_AppMain();

    // Never returns

}

// System Clock configuration
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    // Configure LSE Drive Capability
    // Changed to medium low 2.22.23 to meet the requirements of 
    // AN2867 sections 3.3,3.4 and STM32WL55 DS13293
    // Table 63. Low-speed external user clock characteristics
    __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_MEDIUMLOW);

    // Configure the main internal regulator output voltage
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    // Initializes the CPU, AHB and APB busses clocks
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI|RCC_OSCILLATORTYPE_LSE|RCC_OSCILLATORTYPE_MSI;
    RCC_OscInitStruct.LSEState = RCC_LSE_ON;
    RCC_OscInitStruct.MSIState = RCC_MSI_ON;
    RCC_OscInitStruct.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
#define MSI_FREQUENCY_MHZ   48                              // 48Mhz
    RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_11;      // 48Mhz
    RCC_OscInitStruct.LSIDiv = RCC_LSI_DIV1;
    RCC_OscInitStruct.LSIState = RCC_LSI_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    // Configure the SYSCLKSource, HCLK, PCLK1 and PCLK2 clocks dividers
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK3|RCC_CLOCKTYPE_HCLK
                                  |RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1
                                  |RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_MSI;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.AHBCLK3Divider = RCC_SYSCLK_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
        Error_Handler();
    }

    // Ensure that MSI is wake-up system clock
    __HAL_RCC_WAKEUPSTOP_CLK_CONFIG(RCC_STOP_WAKEUPCLOCK_MSI);

}

// Initialize for GPIO
void MX_GPIO_Init(void)
{

    // Enable all GPIO clocks
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();

    // Default all pins to analog except SWD pins.  (This has a hard-wired
    // assumption that SWDIO_GPIO_Port and SWCLK_GPIO_Port are GPIOA.)
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Pin = GPIO_PIN_All & (~(SWDIO_Pin|SWCLK_Pin));
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = GPIO_PIN_All;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
    HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);

}

// Enable DMA controller clock
void MX_DMA_Init(void)
{

    // DMA controller clock enable
    __HAL_RCC_DMAMUX1_CLK_ENABLE();
    __HAL_RCC_DMA1_CLK_ENABLE();
    __HAL_RCC_DMA2_CLK_ENABLE();

}

// DeInit all known peripherals
void MX_GPIO_DeInit(void)
{
    HAL_NVIC_DisableIRQ(EXTI0_IRQn);
    HAL_NVIC_DisableIRQ(EXTI1_IRQn);
    HAL_NVIC_DisableIRQ(EXTI2_IRQn);
    HAL_NVIC_DisableIRQ(EXTI3_IRQn);
    HAL_NVIC_DisableIRQ(EXTI4_IRQn);
    HAL_NVIC_DisableIRQ(EXTI9_5_IRQn);
    HAL_NVIC_DisableIRQ(EXTI15_10_IRQn);
    MX_AES_DeInit();
    MX_RNG_DeInit();
    MX_USART1_UART_DeInit();
    MX_SPI1_DeInit();
    MX_I2C2_DeInit();
    MX_ADC_DeInit();
}

// Initialize ADC
void MX_ADC_Init(void)
{

    // Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
    hadc.Instance = ADC;

    hadc.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV8;
    hadc.Init.Resolution = ADC_RESOLUTION_12B;
    hadc.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc.Init.ScanConvMode = ADC_SCAN_ENABLE;
    hadc.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    hadc.Init.LowPowerAutoWait = DISABLE;
    hadc.Init.LowPowerAutoPowerOff = DISABLE;
    hadc.Init.ContinuousConvMode = DISABLE;
    hadc.Init.NbrOfConversion = ADC_TOTAL;
    hadc.Init.DiscontinuousConvMode = ENABLE;
    hadc.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc.Init.DMAContinuousRequests = DISABLE;
    hadc.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
    hadc.Init.SamplingTimeCommon1 = ADC_SAMPLETIME_39CYCLES_5;
    hadc.Init.SamplingTimeCommon2 = ADC_SAMPLETIME_160CYCLES_5;
    hadc.Init.OversamplingMode = DISABLE;
    hadc.Init.TriggerFrequencyMode = ADC_TRIGGER_FREQ_HIGH;
    if (HAL_ADC_Init(&hadc) != HAL_OK) {
        Error_Handler();
    }

    // Configure Regular Channels and the VREFINT Channel
    ADC_ChannelConfTypeDef sConfig;
    memset(&sConfig, 0, sizeof(sConfig));

    sConfig.Channel = VREFINT_ADC_Channel;
    sConfig.Rank = VREFINT_ADC_Rank;
    sConfig.SamplingTime = ADC_SAMPLINGTIME_COMMON_1;
    HAL_ADC_ConfigChannel(&hadc, &sConfig);

    sConfig.Channel = A0_ADC_Channel;
    sConfig.Rank = A0_ADC_Rank;
    sConfig.SamplingTime = ADC_SAMPLINGTIME_COMMON_2;
    HAL_ADC_ConfigChannel(&hadc, &sConfig);

#ifdef A1_ENABLED
    sConfig.Channel = A1_ADC_Channel;
    sConfig.Rank = A1_ADC_Rank;
    sConfig.SamplingTime = ADC_SAMPLINGTIME_COMMON_2;
    HAL_ADC_ConfigChannel(&hadc, &sConfig);
#endif

#ifdef A2_ENABLED
    sConfig.Channel = A2_ADC_Channel;
    sConfig.Rank = A2_ADC_Rank;
    sConfig.SamplingTime = ADC_SAMPLINGTIME_COMMON_2;
    HAL_ADC_ConfigChannel(&hadc, &sConfig);
#endif

#ifdef A3_ENABLED
    sConfig.Channel = A3_ADC_Channel;
    sConfig.Rank = A3_ADC_Rank;
    sConfig.SamplingTime = ADC_SAMPLINGTIME_COMMON_2;
    HAL_ADC_ConfigChannel(&hadc, &sConfig);
#endif

    // Enable DMA interrupts
    HAL_NVIC_SetPriority(ADC_DMA_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(ADC_DMA_IRQn);

    peripherals |= PERIPHERAL_ADC;

}

// Deinitialize ADC
void MX_ADC_DeInit(void)
{
    peripherals &= ~PERIPHERAL_ADC;
    HAL_NVIC_DisableIRQ(ADC_DMA_IRQn);
    HAL_ADC_DeInit(&hadc);
}

// Return ADC_COUNT words of values assuming that they're all voltages
bool MX_ADC_Values(uint16_t *wordValues, double *voltageValues, double *vref)
{

    // Init ADC
    MX_ADC_Init();

    // Calibrate
    HAL_ADCEx_Calibration_Start(&hadc);

    // Start DMA
    adcDMACompleted = false;
    memset(adcValues, 0xff, sizeof(adcValues));
    HAL_ADC_Start_DMA(&hadc, (uint32_t *) adcValues, ADC_TOTAL);

    // Perform ADC Conversion, aborting if there is a conversion error
    // because we will lose track of the sequencer position
    for (int i=0; i<250 && !adcDMACompleted; i++) {
        HAL_ADC_Start(&hadc);
        HAL_Delay(10);
    }

    // Stop ADC
    HAL_ADC_Stop(&hadc);

    // Deinit ADC
    MX_ADC_DeInit();

    // Exit if error
    if (!adcDMACompleted) {
    }

    // Calculate vrefint voltage, knowing that vrefint is the first
    uint16_t VrefInt_mVolt = __LL_ADC_CALC_VREFANALOG_VOLTAGE(adcValues[0], LL_ADC_RESOLUTION_12B);
    if (vref != NULL) {
        *vref = ((double) VrefInt_mVolt) / 1000;
    }

    // Calculate return array of voltages, noting that the adcValues array is skewed
    // by 1 because of vrefint being adcValues[0] (see above)
    for (int i=0; i<ADC_COUNT; i++) {
        if (wordValues != NULL) {
            wordValues[i] = adcValues[i+1] << 4;    // 12-bit to 16-bit scale
        }
        if (voltageValues != NULL) {
            double v = __LL_ADC_CALC_DATA_TO_VOLTAGE(VDDA_APPLI, adcValues[i+1], LL_ADC_RESOLUTION_12B);
            voltageValues[i] = ((double) v) / 1000;
        }
    }

    // Done
    return true;

}

// Conversion complete callback in non blocking mode
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    adcDMACompleted = true;
}

// Conversion DMA half-transfer callback in non blocking mode
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)
{
}

// ADC error callback in non blocking mode
void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *hadc)
{
}

// This function calibrates the voltage across its known range of slope and target
//  2018-08-05 5.5v was measured as 5.39862984, and 2.5v was measured at 2.28016664
double calibrateVoltage(double v)
{
#ifdef USE_SPARROW

    // If the dev supplies 3.3v directly to the QWIIC connector, it will work but
    // by bypassing the voltage regulator we can't read the voltage from the regulator's
    // battery monitor.  However, we DO know the voltage because it must be 3.3v
    // to have been supplied at the QWIIC connector.
    if (v < 0.01) {

        v = ((double) VDDA_APPLI) / 1000.0;

    } else {

        // The BAT MON pin of the RP605 requires a multiplier to compute actual voltage
        v = v * BATMON_ADJUSTMENT;

    }

#endif
    return v;
}

// Get a calibrated voltage level using the ADC's A0 line
// Note that the BAT MON is powered by the red LED for current savings.
double MX_ADC_A0_Voltage()
{
#if defined(USE_SPARROW) && defined(USE_LED_TX)
    bool ledWasEnabled = (LED_TX_ON == HAL_GPIO_ReadPin(LED_TX_GPIO_Port, LED_TX_Pin));
    HAL_GPIO_WritePin(LED_TX_GPIO_Port, LED_TX_Pin, LED_TX_ON);

    // Measure the voltage
    double voltage = 0.0;
    double voltageValues[ADC_COUNT];
    if (MX_ADC_Values(NULL, voltageValues, NULL)) {
        voltage = calibrateVoltage(voltageValues[0]);
    }

    if (!ledWasEnabled) {
        HAL_GPIO_WritePin(LED_TX_GPIO_Port, LED_TX_Pin, LED_TX_OFF);
    }

    return voltage;
#else
    return 0.0;
#endif
}

// Init I2C2
void MX_I2C2_Init(void)
{

    // Enable DMA interrupts
    HAL_NVIC_SetPriority(I2C2_RX_DMA_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(I2C2_RX_DMA_IRQn);
    HAL_NVIC_SetPriority(I2C2_TX_DMA_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(I2C2_TX_DMA_IRQn);

    // Configure I2C
    hi2c2.Instance = I2C2;
    hi2c2.Init.Timing = 0x30F03B23;     // Tuned to 100kHz
    hi2c2.Init.OwnAddress1 = 0;
    hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c2.Init.OwnAddress2 = 0;
    hi2c2.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
    hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c2) != HAL_OK) {
        Error_Handler();
    }

    // Configure Analogue filter
    if (HAL_I2CEx_ConfigAnalogFilter(&hi2c2, I2C_ANALOGFILTER_ENABLE) != HAL_OK) {
        Error_Handler();
    }

    // Configure Digital filter
    if (HAL_I2CEx_ConfigDigitalFilter(&hi2c2, 0) != HAL_OK) {
        Error_Handler();
    }

    // Enabled
    peripherals |= PERIPHERAL_I2C2;

}

// DeInit I2C2
void MX_I2C2_DeInit(void)
{
    peripherals &= ~PERIPHERAL_I2C2;
    HAL_NVIC_DisableIRQ(I2C2_RX_DMA_IRQn);
    HAL_NVIC_DisableIRQ(I2C2_TX_DMA_IRQn);
    HAL_I2C_DeInit(&hi2c2);
}

// I2C1 DMA completion events
void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c == &hi2c2) {
        i2c2IOCompletions++;
    }
}
void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c == &hi2c2) {
        i2c2IOCompletions++;
    }
}
void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c == &hi2c2) {
        i2c2IOCompletions++;
    }
}
void HAL_I2C_MemTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c == &hi2c2) {
        i2c2IOCompletions++;
    }
}

bool MY_I2C2_Ping(uint16_t i2cAddress, uint32_t timeoutMs, uint32_t attempts) {
    return (HAL_OK == HAL_I2C_IsDeviceReady(&hi2c2, (uint16_t)(i2cAddress << 1), attempts, timeoutMs));
}

// Receive from a register, and return true for success or false for failure
bool MY_I2C2_ReadRegister(uint16_t i2cAddress, uint8_t Reg, void *data, uint16_t maxdatalen, uint32_t timeoutMs)
{
    uint32_t ioCount = i2c2IOCompletions;
    uint32_t status = HAL_I2C_Mem_Read_DMA(&hi2c2, ((uint16_t)i2cAddress) << 1, (uint16_t)Reg, I2C_MEMADD_SIZE_8BIT, data, maxdatalen);
    if (status != HAL_OK) {
        return false;
    }
    uint32_t waitedMs = 0;
    uint32_t waitGranularityMs = 1;
    bool success = true;
    while (success && ioCount == i2c2IOCompletions) {
        HAL_Delay(waitGranularityMs);
        waitedMs += waitGranularityMs;
        if (timeoutMs != 0 && waitedMs > timeoutMs) {
            success = false;
        }
    }
    return success;
}

// Write a register, and return true for success or false for failure
bool MY_I2C2_WriteRegister(uint16_t i2cAddress, uint8_t Reg, void *data, uint16_t datalen, uint32_t timeoutMs)
{
    uint32_t ioCount = i2c2IOCompletions;
    uint32_t status = HAL_I2C_Mem_Write_DMA(&hi2c2, ((uint16_t)i2cAddress) << 1, (uint16_t)Reg, I2C_MEMADD_SIZE_8BIT, data, datalen);
    if (status != HAL_OK) {
        return false;
    }
    uint32_t waitedMs = 0;
    uint32_t waitGranularityMs = 1;
    bool success = true;
    while (success && ioCount == i2c2IOCompletions) {
        HAL_Delay(waitGranularityMs);
        waitedMs += waitGranularityMs;
        if (timeoutMs != 0 && waitedMs > timeoutMs) {
            success = false;
        }
    }
    return success;
}

// Transmit, and return true for success or false for failure
bool MY_I2C2_Transmit(uint16_t i2cAddress, void *data, uint16_t datalen, uint32_t timeoutMs)
{
    uint32_t ioCount = i2c2IOCompletions;
    uint32_t status = HAL_I2C_Master_Transmit_DMA(&hi2c2, ((uint16_t)i2cAddress) << 1, data, datalen);
    if (status != HAL_OK) {
        return false;
    }
    uint32_t waitedMs = 0;
    uint32_t waitGranularityMs = 1;
    bool success = true;
    while (success && ioCount == i2c2IOCompletions) {
        HAL_Delay(waitGranularityMs);
        waitedMs += waitGranularityMs;
        if (timeoutMs != 0 && waitedMs > timeoutMs) {
            success = false;
        }
    }
    return success;
}

// Receive, and return true for success or false for failure
bool MY_I2C2_Receive(uint16_t i2cAddress, void *data, uint16_t maxdatalen, uint32_t timeoutMs)
{
    uint32_t ioCount = i2c2IOCompletions;
    uint32_t status = HAL_I2C_Master_Receive_DMA(&hi2c2, ((uint16_t)i2cAddress) << 1, data, maxdatalen);
    if (status != HAL_OK) {
        return false;
    }
    uint32_t waitedMs = 0;
    uint32_t waitGranularityMs = 1;
    bool success = true;
    while (success && ioCount == i2c2IOCompletions) {
        HAL_Delay(waitGranularityMs);
        waitedMs += waitGranularityMs;
        if (timeoutMs != 0 && waitedMs > timeoutMs) {
            success = false;
        }
    }
    return success;
}

// SPI1 Initialization
void MX_SPI1_Init(void)
{

    // Enable DMA interrupts
    HAL_NVIC_SetPriority(SPI1_RX_DMA_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(SPI1_RX_DMA_IRQn);
    HAL_NVIC_SetPriority(SPI1_TX_DMA_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(SPI1_TX_DMA_IRQn);

    // SPI1 parameter configuration
    hspi1.Instance = SPI1;
    hspi1.Init.Mode = SPI_MODE_MASTER;
    hspi1.Init.Direction = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize = SPI_DATASIZE_4BIT;
    hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi1.Init.NSS = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
    hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.CRCPolynomial = 7;
    hspi1.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
    hspi1.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
    if (HAL_SPI_Init(&hspi1) != HAL_OK) {
        Error_Handler();
    }

    peripherals |= PERIPHERAL_SPI1;

}

// SPI1 Deinitialization
void MX_SPI1_DeInit(void)
{
    peripherals &= ~PERIPHERAL_SPI1;
    HAL_NVIC_DisableIRQ(SPI1_RX_DMA_IRQn);
    HAL_NVIC_DisableIRQ(SPI1_TX_DMA_IRQn);
    HAL_SPI_DeInit(&hspi1);
}

// SUBGHZ init function
void MX_SUBGHZ_Init(void)
{

    hsubghz.Init.BaudratePrescaler = SUBGHZSPI_BAUDRATEPRESCALER_4;
    if (HAL_SUBGHZ_Init(&hsubghz) != HAL_OK) {
        Error_Handler();
    }
    peripherals |= PERIPHERAL_SUBGHZ;

}

// SUBGHZ de-init function
void MX_SUBGHZ_DeInit(void)
{
    peripherals &= ~PERIPHERAL_SUBGHZ;
    HAL_SUBGHZ_DeInit(&hsubghz);
}

// Encrypt using AES as configured
bool MX_AES_CTR_Encrypt(uint8_t *key, uint8_t *plaintext, uint16_t len, uint8_t *ciphertext)
{
    if ((((uint32_t) plaintext) & 0x03) != 0) {
        return false;
    }
    if ((((uint32_t) ciphertext) & 0x03) != 0) {
        return false;
    }
    memcpy(keyAES, key, sizeof(keyAES));
    MX_AES_Init();
    bool success = HAL_CRYP_Encrypt_IT(&hcryp, (uint32_t *)plaintext, len, (uint32_t *)ciphertext) == HAL_OK;
    if (success) {
        while (HAL_CRYP_GetState(&hcryp) != HAL_CRYP_STATE_READY) ;
    }
    MX_AES_DeInit();
    return success;
}

// Decrypt using AES as configured
bool MX_AES_CTR_Decrypt(uint8_t *key, uint8_t *ciphertext, uint16_t len, uint8_t *plaintext)
{
    if ((((uint32_t) plaintext) & 0x03) != 0) {
        return false;
    }
    if ((((uint32_t) ciphertext) & 0x03) != 0) {
        return false;
    }
    memcpy(keyAES, key, sizeof(keyAES));
    MX_AES_Init();
    bool success = HAL_CRYP_Decrypt_IT(&hcryp, (uint32_t *)ciphertext, len, (uint32_t *)plaintext) == HAL_OK;
    if (success) {
        while (HAL_CRYP_GetState(&hcryp) != HAL_CRYP_STATE_READY) ;
    }
    MX_AES_DeInit();
    return success;
}

// Init AES
void MX_AES_Init()
{
    hcryp.Instance = AES;
    hcryp.Init.DataType = CRYP_DATATYPE_1B;
    hcryp.Init.KeySize = CRYP_KEYSIZE_256B;
    hcryp.Init.pKey = (uint32_t *)keyAES;
    hcryp.Init.pInitVect = AESIV_CTR;
    hcryp.Init.Algorithm = CRYP_AES_CTR;
    hcryp.Init.DataWidthUnit = CRYP_DATAWIDTHUNIT_BYTE;
    hcryp.Init.HeaderWidthUnit = CRYP_HEADERWIDTHUNIT_WORD;
    hcryp.Init.KeyIVConfigSkip = CRYP_KEYIVCONFIG_ALWAYS;
    if (HAL_CRYP_Init(&hcryp) != HAL_OK) {
        Error_Handler();
    }
}

// DeInit AES
void MX_AES_DeInit(void)
{
    HAL_CRYP_DeInit(&hcryp);
}

// Init RNG
void MX_RNG_Init(void)
{
    hrng.Instance = RNG;
    hrng.Init.ClockErrorDetection = RNG_CED_ENABLE;
    if (HAL_RNG_Init(&hrng) != HAL_OK) {
        Error_Handler();
    }
}

// Get a random number
uint32_t MX_RNG_Get()
{

    uint32_t random;
    while (HAL_RNG_GenerateRandomNumber(&hrng, &random) != HAL_OK) {
        HAL_Delay(1);
    }
    return random;
}

// DeInit RNG
void MX_RNG_DeInit(void)
{
    HAL_RNG_DeInit(&hrng);
}

// Microsecond timer
void MX_TIM17_Init(void)
{

    htim17.Instance = TIM17;
    htim17.Init.Prescaler = 0;
    htim17.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim17.Init.Period = 65535;
    htim17.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim17.Init.RepetitionCounter = 0;
    htim17.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&htim17) != HAL_OK) {
        Error_Handler();
    }
    HAL_TIMEx_RemapConfig(&htim17, TIM_TIM17_TI1_MSI);

}

// MX_Delay_Us
void MX_TIM17_DelayUs(uint32_t us)
{
    __HAL_TIM_SET_COUNTER (&htim17, 0);
    __HAL_TIM_ENABLE (&htim17);
    uint32_t ticks = MSI_FREQUENCY_MHZ * us;
    uint32_t base = 0;
    uint32_t prev = 0;
    while (true) {
        uint32_t counter = __HAL_TIM_GET_COUNTER(&htim17);
        if (counter < prev) {
            base += 0x00010000;
        }
        prev = counter;
        if (counter+base >= ticks) {
            break;
        }
    }
    __HAL_TIM_DISABLE(&htim17);
}

// RTC init function
void MX_RTC_Init(void)
{
    RTC_AlarmTypeDef sAlarm = {0};

    // Initialize RTC Only
    hrtc.Instance = RTC;
    hrtc.Init.HourFormat = RTC_HOURFORMAT_24;               // CLOCK
    hrtc.Init.AsynchPrediv = RTC_PREDIV_A;
    hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
    hrtc.Init.OutPutRemap = RTC_OUTPUT_REMAP_NONE;
    hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
    hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
    hrtc.Init.OutPutPullUp = RTC_OUTPUT_PULLUP_NONE;
    hrtc.Init.BinMode = RTC_BINARY_ONLY;
    if (HAL_RTC_Init(&hrtc) != HAL_OK) {
        Error_Handler();
    }

    // Initialize RTC underflow detection interrupt
    if (HAL_RTCEx_SetSSRU_IT(&hrtc) != HAL_OK) {
        Error_Handler();
    }

    // Enable the Alarm A
    sAlarm.BinaryAutoClr = RTC_ALARMSUBSECONDBIN_AUTOCLR_NO;
    sAlarm.AlarmTime.SubSeconds = 0x0;
    sAlarm.AlarmMask = RTC_ALARMMASK_NONE;
    sAlarm.AlarmSubSecondMask = RTC_ALARMSUBSECONDBINMASK_NONE;
    sAlarm.Alarm = RTC_ALARM_A;
    if (HAL_RTC_SetAlarm_IT(&hrtc, &sAlarm, RTC_FORMAT_BCD) != HAL_OK) {
        Error_Handler();
    }

    // Initialized
    peripherals |= PERIPHERAL_RTC;

}

// USART1 init function
void MX_USART1_UART_Init(void)
{

    // Enable DMA interrupts
    HAL_NVIC_SetPriority(USART1_RX_DMA_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(USART1_RX_DMA_IRQn);
    HAL_NVIC_SetPriority(USART1_TX_DMA_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(USART1_TX_DMA_IRQn);

    // Initialize
    huart1.Instance = USART1;
    huart1.Init.BaudRate = USART1_BAUDRATE;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&huart1) != HAL_OK) {
        Error_Handler();
    }

    // Enable FIFO
    if (HAL_UARTEx_SetTxFifoThreshold(&huart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK) {
        Error_Handler();
    }
    if (HAL_UARTEx_SetRxFifoThreshold(&huart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK) {
        Error_Handler();
    }
    if (HAL_UARTEx_EnableFifoMode(&huart1) != HAL_OK) {
        Error_Handler();
    }

    peripherals |= PERIPHERAL_USART1;

}

// Transmit to USART1
void MX_USART1_UART_Transmit(uint8_t *buf, uint32_t len, uint32_t timeoutMs)
{

    // Transmit
    HAL_UART_Transmit_DMA(&huart1, buf, len);

    // Wait, so that the caller won't mess with the buffer while the HAL is using it
    for (uint32_t i=0; i<timeoutMs; i++) {
        HAL_UART_StateTypeDef state = HAL_UART_GetState(&huart1);
        if ((state & HAL_UART_STATE_BUSY_TX) != HAL_UART_STATE_BUSY_TX) {
            break;
        }
        HAL_Delay(1);
    }

}

// USART1 Deinitialization
void MX_USART1_UART_DeInit(void)
{

    // Deinitialized
    peripherals &= ~PERIPHERAL_USART1;

    // Stop any pending DMA, if any
    HAL_UART_DMAStop(&huart1);

    // Reset peripheral
    __HAL_RCC_USART1_FORCE_RESET();
    __HAL_RCC_USART1_RELEASE_RESET();

    // Disable IDLE interrupt
    __HAL_UART_DISABLE_IT(&huart1, UART_IT_IDLE);

    // Deinit
    HAL_UART_DeInit(&huart1);

    // Deinit DMA interrupts
    HAL_NVIC_DisableIRQ(USART1_RX_DMA_IRQn);
    HAL_NVIC_DisableIRQ(USART1_TX_DMA_IRQn);

}

// USART2 init function
void MX_USART2_UART_Init(void)
{

    // Enable DMA interrupts
#ifdef USE_USART2_RX_DMA
    HAL_NVIC_SetPriority(USART2_RX_DMA_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(USART2_RX_DMA_IRQn);
#endif
    HAL_NVIC_SetPriority(USART2_TX_DMA_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(USART2_TX_DMA_IRQn);

    // Initialize
    huart2.Instance = USART2;
    huart2.Init.BaudRate = USART2_BAUDRATE;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart2.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&huart2) != HAL_OK) {
        Error_Handler();
    }

    // Enable FIFO
    if (HAL_UARTEx_SetTxFifoThreshold(&huart2, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK) {
        Error_Handler();
    }
    if (HAL_UARTEx_SetRxFifoThreshold(&huart2, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK) {
        Error_Handler();
    }
    if (HAL_UARTEx_EnableFifoMode(&huart2) != HAL_OK) {
        Error_Handler();
    }

    // Enabled
    peripherals |= PERIPHERAL_USART2;

}

// USART2 suspend function
void MX_USART2_UART_Suspend(void)
{

    // Enable wakeup interrupt from STOP2 (RM0453 Tbl 93)
    // Note that this is the moral equivalent of doing
    // LL_LPUART_EnableIT_WKUP(USART2), however it works
    // on the dual-core processor to say "enable wakeup
    // on either core - whichever is available".
    LL_EXTI_EnableIT_0_31(LL_EXTI_LINE_27);

}

// USART2 resume function
void MX_USART2_UART_Resume(void)
{
    if (HAL_UART_Init(&huart2) != HAL_OK) {
        Error_Handler();
    }
    if (HAL_DMA_Init(&hdma_usart2_tx) != HAL_OK) {
        Error_Handler();
    }
}

// Transmit to USART2
void MX_USART2_UART_Transmit(uint8_t *buf, uint32_t len, uint32_t timeoutMs)
{

    // Transmit
    HAL_UART_Transmit_DMA(&huart2, buf, len);

    // Wait, so that the caller won't mess with the buffer while the HAL is using it
    for (uint32_t i=0; i<timeoutMs; i++) {
        HAL_UART_StateTypeDef state = HAL_UART_GetState(&huart2);
        if ((state & HAL_UART_STATE_BUSY_TX) != HAL_UART_STATE_BUSY_TX) {
            break;
        }
        HAL_Delay(1);
    }

}

// USART2 Deinitialization
void MX_USART2_UART_DeInit(void)
{

    // Deinitialized
    peripherals &= ~PERIPHERAL_USART2;

    // Stop any pending DMA, if any
    HAL_UART_DMAStop(&huart2);

    // Reset peripheral
    __HAL_RCC_USART2_FORCE_RESET();
    __HAL_RCC_USART2_RELEASE_RESET();

    // Disable IDLE interrupt
    __HAL_UART_DISABLE_IT(&huart2, UART_IT_IDLE);

    // Deinit
    HAL_UART_DeInit(&huart2);

    // Deinit DMA interrupts
#ifdef USE_USART2_RX_DMA
    HAL_NVIC_DisableIRQ(USART2_RX_DMA_IRQn);
#endif
    HAL_NVIC_DisableIRQ(USART2_TX_DMA_IRQn);

}

// LPUART1 Initialization Function
void MX_LPUART1_UART_Init(void)
{

    hlpuart1.Instance = LPUART1;
    hlpuart1.Init.BaudRate = LPUART1_BAUDRATE;
    hlpuart1.Init.WordLength = UART_WORDLENGTH_8B;
    hlpuart1.Init.StopBits = UART_STOPBITS_1;
    hlpuart1.Init.Parity = UART_PARITY_NONE;
    hlpuart1.Init.Mode = UART_MODE_TX_RX;
    hlpuart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    hlpuart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    hlpuart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    hlpuart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    hlpuart1.FifoMode = UART_FIFOMODE_DISABLE;
    if (HAL_UART_Init(&hlpuart1) != HAL_OK) {
        Error_Handler();
    }
    if (HAL_UARTEx_SetTxFifoThreshold(&hlpuart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK) {
        Error_Handler();
    }
    if (HAL_UARTEx_SetRxFifoThreshold(&hlpuart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK) {
        Error_Handler();
    }
    if (HAL_UARTEx_DisableFifoMode(&hlpuart1) != HAL_OK) {
        Error_Handler();
    }

    peripherals |= PERIPHERAL_LPUART1;

}

// LPUART1 suspend function
void MX_LPUART1_UART_Suspend(void)
{

    // Clear OVERRUN flag
    LL_LPUART_ClearFlag_ORE(LPUART1);

    // Make sure that no LPUART transfer is on-going
    while (LL_LPUART_IsActiveFlag_BUSY(LPUART1) == 1) {};

    // Make sure that LPUART is ready to receive
    while (LL_LPUART_IsActiveFlag_REACK(LPUART1) == 0) {};

    // Configure LPUART1 transfer interrupts by clearing the WUF
    // flag and enabling the UART Wake Up from stop mode interrupt
    LL_LPUART_ClearFlag_WKUP(LPUART1);
    LL_LPUART_EnableIT_WKUP(LPUART1);

    // Enable Wake Up From Stop
    LL_LPUART_EnableInStopMode(LPUART1);

    // Unmask wakeup with Interrupt request from LPUART1
    LL_EXTI_EnableIT_0_31(LL_EXTI_LINE_28);

}

// LPUART1 resume function
void MX_LPUART1_UART_Resume(void)
{
}

// Transmit to LPUART1
void MX_LPUART1_UART_Transmit(uint8_t *buf, uint32_t len, uint32_t timeoutMs)
{

    // Transmit
    HAL_UART_Transmit_IT(&hlpuart1, buf, len);

    // Wait, so that the caller won't mess with the buffer while the HAL is using it
    for (uint32_t i=0; i<timeoutMs; i++) {
        HAL_UART_StateTypeDef state = HAL_UART_GetState(&hlpuart1);
        if ((state & HAL_UART_STATE_BUSY_TX) != HAL_UART_STATE_BUSY_TX) {
            break;
        }
        HAL_Delay(1);
    }

}

// LPUART1 De-Initialization Function
void MX_LPUART1_UART_DeInit(void)
{
    peripherals &= ~PERIPHERAL_LPUART1;
    HAL_UART_DeInit(&hlpuart1);
}

// Get the image size
uint32_t MX_Image_Size()
{
#if defined( __ICCARM__ )   // IAR
    return (uint32_t) (&ROM_CONTENT$$Limit) - FLASH_BASE;
#else
    return (uint32_t) (&_highest_used_rom) - FLASH_BASE;
#endif
}

// Get the RAM limits
uint32_t MX_Heap_Size(uint8_t **base)
{
#if defined( __ICCARM__ )   // IAR
    uint8_t *heap = (uint8_t *) &HEAP$$Base;
    uint8_t *heapEnd = (uint8_t *) &HEAP$$Limit;
#else
    uint8_t *heap = (uint8_t *)&_end;
    const uint32_t stack_limit = (uint32_t)&_estack - (uint32_t)&_Min_Stack_Size;
    const uint8_t *heapEnd = (uint8_t *)stack_limit;
#endif
    uint32_t heapSize = (uint32_t) (heapEnd - heap);
    if (base != NULL) {
        *base = heap;
    }
    return heapSize;
}

// Get the currently active peripherals
void MY_ActivePeripherals(char *buf, uint32_t buflen)
{
    *buf = '\0';
    if ((peripherals & PERIPHERAL_RNG) != 0) {
        strlcat(buf, "RNG ", buflen);
    }
    if ((peripherals & PERIPHERAL_RTC) != 0) {
        strlcat(buf, "RTC ", buflen);
    }
    if ((peripherals & PERIPHERAL_SUBGHZ) != 0) {
        strlcat(buf, "SUBGHZ ", buflen);
    }
    if ((peripherals & PERIPHERAL_ADC) != 0) {
        strlcat(buf, "ADC ", buflen);
    }
    if ((peripherals & PERIPHERAL_ADCDMA) != 0) {
        strlcat(buf, "ADCDMA ", buflen);
    }
    if ((peripherals & PERIPHERAL_LPUART1) != 0) {
        strlcat(buf, "LPUART1 ", buflen);
    }
    if ((peripherals & PERIPHERAL_USART1) != 0) {
        strlcat(buf, "USART1 ", buflen);
    }
    if ((peripherals & PERIPHERAL_USART2) != 0) {
        strlcat(buf, "USART2 ", buflen);
    }
    if ((peripherals & PERIPHERAL_CRYP) != 0) {
        strlcat(buf, "CRYP ", buflen);
    }
    if ((peripherals & PERIPHERAL_I2C2) != 0) {
        strlcat(buf, "I2C2 ", buflen);
    }
    if ((peripherals & PERIPHERAL_SPI1) != 0) {
        strlcat(buf, "SPI1 ", buflen);
    }
    if ((peripherals & PERIPHERAL_SPI1DMA) != 0) {
        strlcat(buf, "SPI1DMA ", buflen);
    }
    if ((peripherals & PERIPHERAL_TIM17) != 0) {
        strlcat(buf, "TIM17 ", buflen);
    }

}
