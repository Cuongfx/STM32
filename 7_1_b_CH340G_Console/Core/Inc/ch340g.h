#ifndef CH340G_H
#define CH340G_H

#include "main.h"

#define CH340G_RX_LINE_SIZE 40U


void              CH340G_Init(UART_HandleTypeDef *huart);
HAL_StatusTypeDef CH340G_Send(const char *str);
HAL_StatusTypeDef CH340G_Printf(const char *fmt, ...);
void CH340G_RxCallback(UART_HandleTypeDef *huart);

uint8_t CH340G_ReadLine(char *line, uint16_t size);
void    CH340G_ErrorCallback(UART_HandleTypeDef *huart);

#endif