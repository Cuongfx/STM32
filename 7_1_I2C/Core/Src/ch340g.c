#include "ch340g.h"
#include <stdarg.h>   // va_list, va_start, va_end
#include <stdio.h>    // vsnprintf
#include <string.h>   // strlen, strncpy

#define CH340G_TX_BUF_SIZE  64U
#define CH340G_TIMEOUT      100U   // ms

static UART_HandleTypeDef *ch340g_uart;       // set by CH340G_Init

static uint8_t rx_byte;                       // the one byte HAL receives into
static char rx_line[CH340G_RX_LINE_SIZE];     // line being assembled
static uint16_t rx_len;                       // characters collected so far
static volatile uint8_t line_ready;           // ISR sets it, main loop clears it

#define CH340G_RX_IDLE_MS  50U                // quiet time that marks the end of a command

static volatile uint32_t rx_tick;             // time of the last received character

void CH340G_Init(UART_HandleTypeDef *huart)
{
  ch340g_uart = huart;
  HAL_UART_Receive_IT(ch340g_uart, &rx_byte, 1);   // arm reception of ONE byte
}

HAL_StatusTypeDef CH340G_Send(const char *str)
{
  return HAL_UART_Transmit(ch340g_uart, (uint8_t *)str,
                           (uint16_t)strlen(str), CH340G_TIMEOUT);
}

HAL_StatusTypeDef CH340G_Printf(const char *fmt, ...)
{
  char buf[CH340G_TX_BUF_SIZE];

  va_list args;
  va_start(args, fmt);                         // start reading the arguments after fmt
  int n = vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);                                // always pair with va_start

  if (n < 0)
  {
    return HAL_ERROR;                          // formatting failed
  }
  if (n >= (int)sizeof(buf))
  {
    n = (int)sizeof(buf) - 1;                  // output was cut to fit the buffer
  }

  return HAL_UART_Transmit(ch340g_uart, (uint8_t *)buf,
                           (uint16_t)n, CH340G_TIMEOUT);
}

void CH340G_RxCallback(UART_HandleTypeDef *huart)
{
  if (huart != ch340g_uart)
  {
    return;   // not our UART
  }

  if (!line_ready)   // while a line waits for the main loop, drop new bytes
  {
    if (rx_byte == '\r' || rx_byte == '\n')
    {
      if (rx_len > 0)            // ignore empty lines (CR followed by LF)
      {
        rx_line[rx_len] = '\0';
        line_ready = 1;
      }
    }
    else if (rx_len < CH340G_RX_LINE_SIZE - 1)
    {
      rx_line[rx_len++] = (char)rx_byte;   // too-long lines are cut, extra chars dropped
      rx_tick = HAL_GetTick();             // remember when the last character arrived
    }
  }

  HAL_UART_Receive_IT(ch340g_uart, &rx_byte, 1);   // re-arm: HAL receives one byte per call
}

void CH340G_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart != ch340g_uart)
  {
    return;
  }
  __HAL_UART_CLEAR_OREFLAG(huart);                 // overrun: a byte arrived before the last was read
  HAL_UART_Receive_IT(ch340g_uart, &rx_byte, 1);   // overrun stops reception, so re-arm
}

uint8_t CH340G_ReadLine(char *line, uint16_t size)
{
  __disable_irq();   // keep the ISR from adding a character while we check
  if (!line_ready && rx_len > 0 && (HAL_GetTick() - rx_tick) >= CH340G_RX_IDLE_MS)
  {
    rx_line[rx_len] = '\0';   // no CR/LF came, but the line has gone quiet: it is complete
    line_ready = 1;
  }
  __enable_irq();

  if (!line_ready)
  {
    return 0;
  }

  strncpy(line, rx_line, size - 1);
  line[size - 1] = '\0';

  rx_len = 0;        // reset first, then release the buffer to the ISR
  line_ready = 0;
  return 1;
}

