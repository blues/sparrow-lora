// Copyright 2021 Blues Inc.  All rights reserved.
// Use of this source code is governed by licenses granted by the
// copyright holder including that found in the LICENSE file.

#include <stdio.h>
#include "main.h"

// IAR-only method of writing to debug terminal without loading printf library
#if defined( __ICCARM__ )
#include "LowLevelIOInterface.h"
#endif

// Receive buffer and ISRs
#if (DEBUGGER_ON_USART2||DEBUGGER_ON_LPUART1)
void dbgReceivedByteISR(UART_HandleTypeDef *huart);
void dbgRestartReceive(UART_HandleTypeDef *huart);
bool dbgReceiveOverrun = false;
uint32_t dbgReceiveFillIndex = 0;
uint32_t dbgReceiveDrainIndex = 0;
uint8_t dbgReceiveBuffer[500];
bool dbgDisableOutput = false;
#endif

// See if the debugger is active
bool MX_DBG_Active()
{
    return ((CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk) != 0);
}

// Output a message to the console, a line at a time because
// the STM32CubeIDE doesn't recognize \n as doing an implicit
// carriage return.
void MX_DBG(const char *message, size_t length)
{

    // Filter out \r and transform \n into \r\n
#if (DEBUGGER_ON_USART2||DEBUGGER_ON_LPUART1)
    const char *p = message;
    size_t chunkSize = 100;
    size_t left = length;
    while (!dbgDisableOutput && left) {
        size_t len = left > chunkSize ? chunkSize : left;
        const char *cr = (const char *) memchr(p, '\r', len);
        const char *nl = (const char *) memchr(p, '\n', len);
        if (cr != NULL && (nl == NULL || nl > cr)) {
            len = (size_t) (cr - p);
#if DEBUGGER_ON_USART2
            MX_USART2_UART_Transmit((uint8_t *)p, len);
#endif
#if DEBUGGER_ON_LPUART1
            MX_LPUART1_UART_Transmit((uint8_t *)p, len);
#endif
            len++;
        } else {
            if (nl == NULL) {
#if DEBUGGER_ON_USART2
                MX_USART2_UART_Transmit((uint8_t *)p, len);
#endif
#if DEBUGGER_ON_LPUART1
                MX_LPUART1_UART_Transmit((uint8_t *)p, len);
#endif
            } else {
                len = (size_t) (nl - p);
#if DEBUGGER_ON_USART2
                MX_USART2_UART_Transmit((uint8_t *)p, len);
                MX_USART2_UART_Transmit((uint8_t *)"\r\n", 2);
#endif
#if DEBUGGER_ON_LPUART1
                MX_LPUART1_UART_Transmit((uint8_t *)p, len);
                MX_LPUART1_UART_Transmit((uint8_t *)"\r\n", 2);
#endif
                len++;
            }
        }
        p += len;
        left -= len;
    }
#endif

    // On IAR only, output to debug console without the 7KB overhead of
    // printf("%.*s", (int) length, message)
#if 0   // 2021-10-01 DISABLED because it is so slow that it messes up
        // LoRa RX window timing, making the module look flaky during debug
#if defined( __ICCARM__ )
    if (MX_DBG_Active()) {
        __dwrite(_LLIO_STDOUT, (const unsigned char *)message, length);
    }
#endif
#endif

}

// Prepare for going into stop2 mode
void MX_DBG_Suspend()
{
#if DEBUGGER_ON_USART2
    MX_USART2_UART_Suspend();
#endif
#if DEBUGGER_ON_LPUART1
    MX_LPUART1_UART_Suspend();
#endif
}

// Resume after coming out of STOP2 mode
void MX_DBG_Resume()
{
#if DEBUGGER_ON_USART2
    MX_USART2_UART_Resume();
    dbgRestartReceive(&huart2);
#endif
#if DEBUGGER_ON_LPUART1
    MX_LPUART1_UART_Resume();
    dbgRestartReceive(&hlpuart1);
#endif
}

// Get pending received bytes and reset the counter.
bool MX_DBG_Available(void)
{
    uint32_t drainIndex = dbgReceiveDrainIndex;
    if (dbgReceiveFillIndex != drainIndex) {
        return true;
    }
    return false;
}

// Get pending received bytes and reset the counter.
uint8_t MX_DBG_Receive(bool *underrun, bool *overrun)
{
    if (overrun != NULL) {
        *overrun = dbgReceiveOverrun;
        dbgReceiveOverrun = false;
    }
    uint32_t drainIndex = dbgReceiveDrainIndex;
    if (dbgReceiveFillIndex == drainIndex) {
        if (underrun != NULL) {
            *underrun = true;
        }
        return 0;
    }
    if (underrun != NULL) {
        *underrun = false;
    }
    uint32_t nextIndex = dbgReceiveDrainIndex + 1;
    if (nextIndex >= sizeof(dbgReceiveBuffer)) {
        nextIndex = 0;
    }
    volatile uint8_t databyte = dbgReceiveBuffer[dbgReceiveDrainIndex];
    dbgReceiveDrainIndex = nextIndex;
    return databyte;
}

// ISR for debug character receive
#if (DEBUGGER_ON_USART2||DEBUGGER_ON_LPUART1)
void dbgRestartReceive(UART_HandleTypeDef *huart)
{
    // Use zero/nonzero as an indicator of a valid byte having been received
    dbgReceiveBuffer[dbgReceiveFillIndex] = 0;
#if DEBUGGER_ON_USART2
#ifdef USE_USART2_RX_DMA
    HAL_UART_Receive_DMA(huart, &dbgReceiveBuffer[dbgReceiveFillIndex], 1);
#else
    HAL_UART_Receive_IT(huart, &dbgReceiveBuffer[dbgReceiveFillIndex], 1);
#endif
#endif
#if DEBUGGER_ON_LPUART1
    HAL_UART_Receive_IT(huart, &dbgReceiveBuffer[dbgReceiveFillIndex], 1);
#endif
}
void dbgReceivedByteISR(UART_HandleTypeDef *huart)
{

    // Ingest the byte
    uint32_t nextIndex = dbgReceiveFillIndex + 1;
    if (nextIndex >= sizeof(dbgReceiveBuffer)) {
        nextIndex = 0;
    }
    if (nextIndex == dbgReceiveDrainIndex) {
        dbgReceiveOverrun = true;
    } else {
        dbgReceiveFillIndex = nextIndex;
    }
    dbgRestartReceive(huart);

    // Notify the app that an input interrupt occurred
    // If we actually received a byte.
    MX_AppISR(0);

}
#endif

// Put debug to sleep if we haven't yet had a keystroke
void MX_DBG_Disable()
{
    dbgDisableOutput = true;
}

// See if output is currently enabled
bool MX_DBG_Enabled()
{
    return !dbgDisableOutput;
}

// Wake up debug
void MX_DBG_Enable()
{
    dbgDisableOutput = false;
}

// Init debugging
void MX_DBG_Init(void)
{

    // Initialize debug output
#if DEBUGGER_ON_USART2

    // Init UART
    MX_USART2_UART_Init();

    // Register a receive callback and initiate the receive
    HAL_UART_RegisterCallback(&huart2, HAL_UART_RX_COMPLETE_CB_ID, dbgReceivedByteISR);
    dbgRestartReceive(&huart2);

#endif

    // Initialize debug output
#if DEBUGGER_ON_LPUART1

    // Init UART
    MX_LPUART1_UART_Init();

    // Register a receive callback and initiate the receive
    HAL_UART_RegisterCallback(&hlpuart1, HAL_UART_RX_COMPLETE_CB_ID, dbgReceivedByteISR);
    dbgRestartReceive(&hlpuart1);

#endif

    // Enable Radio GPIO debugging
#if (DEBUGGER_ON && DEBUGGER_RADIO_DBG_GPIO)

    GPIO_InitTypeDef  GPIO_InitStruct = {0};
    GPIO_InitStruct.Mode   = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull   = GPIO_PULLUP;
    GPIO_InitStruct.Speed  = GPIO_SPEED_FREQ_VERY_HIGH;

    GPIO_InitStruct.Pin    = DBG_LINE1_Pin;
    HAL_GPIO_Init(DBG_LINE1_GPIO_Port, &GPIO_InitStruct);
    GPIO_InitStruct.Pin    = DBG_LINE2_Pin;
    HAL_GPIO_Init(DBG_LINE2_GPIO_Port, &GPIO_InitStruct);
    GPIO_InitStruct.Pin    = DBG_LINE3_Pin;
    HAL_GPIO_Init(DBG_LINE3_GPIO_Port, &GPIO_InitStruct);
    GPIO_InitStruct.Pin    = DBG_LINE4_Pin;
    HAL_GPIO_Init(DBG_LINE4_GPIO_Port, &GPIO_InitStruct);

    HAL_GPIO_WritePin(DBG_LINE1_GPIO_Port, DBG_LINE1_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(DBG_LINE2_GPIO_Port, DBG_LINE2_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(DBG_LINE3_GPIO_Port, DBG_LINE3_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(DBG_LINE4_GPIO_Port, DBG_LINE4_Pin, GPIO_PIN_RESET);

#endif  // Radio debugging

    // Enable or disable debug mode
    if (MX_DBG_Active()) {
        LL_EXTI_EnableIT_32_63(LL_EXTI_LINE_46);    // RM0453 Table 93 38.3.4
        HAL_DBGMCU_EnableDBGSleepMode();
        HAL_DBGMCU_EnableDBGStopMode();
        HAL_DBGMCU_EnableDBGStandbyMode();
    } else {
        GPIO_InitTypeDef GPIO_InitStruct = {0};
        GPIO_InitStruct.Mode   = GPIO_MODE_ANALOG;
        GPIO_InitStruct.Pull   = GPIO_NOPULL;
        GPIO_InitStruct.Pin    = SWCLK_Pin;
        HAL_GPIO_Init(SWCLK_GPIO_Port, &GPIO_InitStruct);
        GPIO_InitStruct.Pin    = SWDIO_Pin;
        HAL_GPIO_Init(SWDIO_GPIO_Port, &GPIO_InitStruct);
        HAL_DBGMCU_DisableDBGSleepMode();
        HAL_DBGMCU_DisableDBGStopMode();
        HAL_DBGMCU_DisableDBGStandbyMode();
    }

}
