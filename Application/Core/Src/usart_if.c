// Copyright 2021 Blues Inc.  All rights reserved.
// Use of this source code is governed by licenses granted by the
// copyright holder including that found in the LICENSE file.

#include "main.h"
#include "stm32_adv_trace.h"

// Init the UART and associated DMA.
// cb TxCpltCallback
UTIL_ADV_TRACE_Status_t vcom_Init(void (*cb)(void *));

// init receiver of vcom
// RxCb callback when Rx char is received
UTIL_ADV_TRACE_Status_t vcom_ReceiveInit(void (*RxCb)(uint8_t *rxChar, uint16_t size, uint8_t error));

// DeInit the UART and associated DMA.
UTIL_ADV_TRACE_Status_t vcom_DeInit(void);

// send buffer \p p_data of size \p size to vcom in polling mode
// p_data data to be sent
// size of buffer p_data to be sent
void vcom_Trace(uint8_t *p_data, uint16_t size);

// send buffer \p p_data of size \p size to vcom using DMA
// p_data data to be sent
// size of buffer p_data to be sent
UTIL_ADV_TRACE_Status_t vcom_Trace_DMA(uint8_t *p_data, uint16_t size);

// last byte has been sent on the uart line
void vcom_IRQHandler(void);

// last byte has been sent from memory to uart data register
void vcom_DMA_TX_IRQHandler(void);

// Handles
extern DMA_HandleTypeDef hdma_usart2_tx;
extern UART_HandleTypeDef huart2;

// Character buffer
uint8_t charRx;

// Trace driver callbacks handler
const UTIL_ADV_TRACE_Driver_s UTIL_TraceDriver = {
    vcom_Init,
    vcom_DeInit,
    vcom_ReceiveInit,
    vcom_Trace_DMA,
};

// Init virtual comms
UTIL_ADV_TRACE_Status_t vcom_Init(void (*cb)(void *))
{
    MX_DBG_Init();
    MX_DBG_TxCpltCallback(cb);
    return UTIL_ADV_TRACE_OK;
}

// DeInit virtual comms
UTIL_ADV_TRACE_Status_t vcom_DeInit(void)
{
    MX_DBG_Init();
    return UTIL_ADV_TRACE_OK;
}

// Trace over the vcom port with interrupts
void vcom_Trace(uint8_t *p_data, uint16_t size)
{
    MX_DBG((const char *)p_data, (size_t)size, 0);
}

// Trace with DMA
UTIL_ADV_TRACE_Status_t vcom_Trace_DMA(uint8_t *p_data, uint16_t size)
{
    MX_DBG((const char *)p_data, (size_t)size, 0);
    return UTIL_ADV_TRACE_OK;
}

// Receive
UTIL_ADV_TRACE_Status_t vcom_ReceiveInit(void (*RxCb)(uint8_t *rxChar, uint16_t size, uint8_t error))
{
    MX_DBG_RxCallback(RxCb);
    return UTIL_ADV_TRACE_OK;
}
