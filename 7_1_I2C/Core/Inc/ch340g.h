#ifndef CH340G_H
#define CH340G_H

#include "main.h"   // HAL types: UART_HandleTypeDef, HAL_StatusTypeDef

#define CH340G_RX_LINE_SIZE  40U   // longest command accepted, including the terminator

void              CH340G_Init(UART_HandleTypeDef *huart);
HAL_StatusTypeDef CH340G_Send(const char *str);
HAL_StatusTypeDef CH340G_Printf(const char *fmt, ...);

uint8_t CH340G_ReadLine(char *line, uint16_t size);       // 1 = a full line was copied into line
void    CH340G_RxCallback(UART_HandleTypeDef *huart);     // forward HAL_UART_RxCpltCallback here
void    CH340G_ErrorCallback(UART_HandleTypeDef *huart);  // forward HAL_UART_ErrorCallback here

#endif