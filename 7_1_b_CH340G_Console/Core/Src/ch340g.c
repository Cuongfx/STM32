#include "ch340g.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static UART_HandleTypeDef *ch340g_uart;
static uint8_t rx_byte;

void CH340G_Init(UART_HandleTypeDef *huart){
    ch340g_uart = huart;
    HAL_UART_Receive_IT(huart, &rx_byte, 1);
}

HAL_StatusTypeDef CH340G_Send(const char *str){
    return HAL_UART_Transmit(ch340g_uart, (uint8_t *)str, strlen(str), HAL_MAX_DELAY);
}

HAL_StatusTypeDef CH340G_Printf(const char *fmt, ...)
{
    char buf[64];
    va_list args;

    va_start(args, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (n < 0)
    {
        return HAL_ERROR;
    }
    if (n >= (int)sizeof(buf))
    {
        n = sizeof(buf) - 1;          // text was cut: send only what fits
    }
    return HAL_UART_Transmit(ch340g_uart, (uint8_t *)buf, n, HAL_MAX_DELAY);
}

void CH340G_RxCallback(UART_HandleTypeDef *huart){
    if (huart!=ch340g_uart){
        return;
    }
    HAL_UART_Transmit(ch340g_uart, &rx_byte, 1, 10);
    HAL_UART_Receive_IT(ch340g_uart, &rx_byte, 1);
}