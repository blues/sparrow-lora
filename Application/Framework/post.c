// Copyright 2022 Blues Inc.  All rights reserved.
// Use of this source code is governed by licenses granted by the
// copyright holder including that found in the LICENSE file.

#include <stdio.h>
#include <stdlib.h>
#include "framework.h"
#include "board.h"
#include "main.h"

// Error message buffer
static char err[48] = {0};
static char pinname[8] = {0};

// Forwards
char *pinName(void *port, uint16_t pin);
char *pinStateName(void *port, uint16_t pin);
bool pinIsFloat(char *alias, void *port, uint16_t pin);
bool pinIsLow(char *alias, void *port, uint16_t pin);
bool pinIsHigh(char *alias, void *port, uint16_t pin);
char *simple_utoa(unsigned int i, char *out);
char *postTest(uint32_t whatToTest);

// Power-on self test, returns NULL or error message.  Note
// that this does not test LPUART1/USART2 because that's the
// port that is connected to the debugger to get POST output.
char *post(uint32_t whatToTest)
{

    // Make sure that output is enabled
    MX_DBG_Enable();

    // Deinit GPIOs and disable IRQ's so that it doesn't confuse below
    MX_GPIO_DeInit();

    // Do the test
    char *failReason = postTest(whatToTest);
    if (failReason == NULL) {
        APP_PRINTF("{\"sensor\":\"%s\"}\r\n\r\n", ourAddressText);
        HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin, GPIO_PIN_RESET);
        for (;;) {
            HAL_Delay(150);
            HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_SET);
            HAL_Delay(150);
            HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_RESET);
        }
    } else {
        APP_PRINTF("{\"sensor\":\"%s\",\"err\":\"%s\"}\r\n\r\n", ourAddressText, failReason);
        HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin, GPIO_PIN_RESET);
        for (;;) {
            HAL_Delay(150);
            HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_SET);
            HAL_Delay(150);
            HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_RESET);
        }

    }
}

// Perform the body of the test
char *postTest(uint32_t whatToTest)
{

    // If the BME was present at init, it passes
    if ((whatToTest & POST_BME) != 0 && appSKU() != SKU_REFERENCE) {
        strlcpy(err, "BME failure", sizeof(err));
        return err;
    }

    // Test GPIOs
    if ((whatToTest & POST_GPIO) != 0) {
        if (!pinIsFloat("SPI1_CS", SPI1_CS_GPIO_Port, SPI1_CS_Pin)) {
            return err;
        }
        if (appSKU() != SKU_REFERENCE) {
            if (!pinIsFloat("SPI1_MISO", SPI1_MISO_GPIO_Port, SPI1_MISO_Pin)) {
                return err;    // used for PIR serial in
            }
            if (!pinIsFloat("SPI1_MOSI", SPI1_MOSI_GPIO_Port, SPI1_MOSI_Pin)) {
                return err;    // used for PIR direct link
            }
            if (!pinIsFloat("SPI1_SCK", SPI1_SCK_GPIO_Port, SPI1_SCK_Pin)) {
                return err;    // used for BME power
            }
        }

        if (appSKU() != SKU_REFERENCE) {
            if (!pinIsFloat("I2C2_SDA", I2C2_SDA_GPIO_Port, I2C2_SDA_Pin)) {
                return err;
            }
            if (!pinIsFloat("I2C2_SCL", I2C2_SCL_GPIO_Port, I2C2_SCL_Pin)) {
                return err;
            }
        }

        if (!pinIsHigh("USART1_RX", USART1_RX_GPIO_Port, USART1_RX_Pin)) {
            return err;
        }
        if (!pinIsFloat("USART1_TX", USART1_TX_GPIO_Port, USART1_TX_Pin)) {
            return err;
        }

        if (!pinIsHigh("BUTTON1", BUTTON1_GPIO_Port, BUTTON1_Pin)) {
            return err;
        }

        if (!pinIsFloat("A1", A1_GPIO_Port, A1_Pin)) {
            return err;
        }
        if (!pinIsFloat("A2", A2_GPIO_Port, A2_Pin)) {
            return err;
        }
        if (!pinIsFloat("A3", A3_GPIO_Port, A3_Pin)) {
            return err;
        }

    }

    return NULL;

}

// Name of pin
char *pinName(void *port, uint16_t pin)
{
    strlcpy(pinname, "P", sizeof(pinname));
    if (port == GPIOA) {
        strlcat(pinname, "A", sizeof(pinname));
    } else if (port == GPIOB) {
        strlcat(pinname, "B", sizeof(pinname));
    } else if (port == GPIOC) {
        strlcat(pinname, "C", sizeof(pinname));
    } else if (port == GPIOH) {
        strlcat(pinname, "H", sizeof(pinname));
    } else {
        strlcat(pinname, "?", sizeof(pinname));
    }
    int i;
    for (i=0; i<16; i++) {
        if (((pin >> i) & 1) != 0) {
            break;
        }
    }
    char chstr[4];
    simple_utoa((unsigned)i, chstr);
    strlcat(pinname, chstr, sizeof(pinname));
    return pinname;
}

// Name of pin state
char *pinStateName(void *port, uint16_t pin)
{
    int state = pinstate(port, pin);
    switch (state) {
    case PINSTATE_HIGH:
        return "HIGH";
    case PINSTATE_LOW:
        return "LOW";
    case PINSTATE_FLOAT:
        return "FLOATING";
    }
    return "?";
}

// High assertion
bool pinIsHigh(char *alias, void *port, uint16_t pin)
{
    if (pinstate(port, pin) == PINSTATE_HIGH) {
        return true;
    }
    strlcpy(err, alias, sizeof(err));
    strlcat(err, " ", sizeof(err));
    strlcat(err, pinName(port, pin), sizeof(err));
    strlcat(err, " is ", sizeof(err));
    strlcat(err, pinStateName(port, pin), sizeof(err));
    strlcat(err, " instead of HIGH", sizeof(err));
    return false;
}

// Low assertion
bool pinIsLow(char *alias, void *port, uint16_t pin)
{
    if (pinstate(port, pin) == PINSTATE_LOW) {
        return true;
    }
    strlcpy(err, alias, sizeof(err));
    strlcat(err, " ", sizeof(err));
    strlcat(err, pinName(port, pin), sizeof(err));
    strlcat(err, " is ", sizeof(err));
    strlcat(err, pinStateName(port, pin), sizeof(err));
    strlcat(err, " instead of LOW", sizeof(err));
    return false;
}

// Float assertion
bool pinIsFloat(char *alias, void *port, uint16_t pin)
{
    if (pinstate(port, pin) == PINSTATE_FLOAT) {
        return true;
    }
    strlcpy(err, alias, sizeof(err));
    strlcat(err, " ", sizeof(err));
    strlcat(err, pinName(port, pin), sizeof(err));
    strlcat(err, " is ", sizeof(err));
    strlcat(err, pinStateName(port, pin), sizeof(err));
    strlcat(err, " instead of FLOAT", sizeof(err));
    return false;
}

// Simple base 10 utoa
char *simple_utoa(unsigned int i, char *out)
{
    char* p = out;
    int shifter = i;
    do {
        ++p;
        shifter = shifter/10;
    } while(shifter);
    out = p;
    *out = '\0';
    do {
        *--p = '0' + (i%10);
        i = i/10;
    } while(i);
    return out;
}
