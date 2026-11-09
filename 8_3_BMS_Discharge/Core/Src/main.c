/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "bq76920.h"
#include "discharge.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
char buf[200];
DIS_Pack pack;
uint8_t  rx_byte;                       /* one byte at a time from USART2 */
char     rx_tail[16];                   /* the last characters received */
volatile uint8_t command;               /* set by the UART callback: 1 = start, 2 = stop */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void send(const char *s)
{
  HAL_UART_Transmit(&huart2, (uint8_t *)s, strlen(s), HAL_MAX_DELAY);
}

/* One line with every cell, the pack total and the pack current. */
static void print_status(void)
{
  int n = 0;
  for (uint8_t k = 0; k < pack.count; k++)
  {
    n += snprintf(buf + n, sizeof(buf) - n, "Cell%u=%u ", k + 1, pack.mv[k]);
  }
  n += snprintf(buf + n, sizeof(buf) - n, "mV  Pack=%lu mV  I=%ld mA  %s",
                (unsigned long)pack.pack_mv, (long)pack.current_ma,
                pack.running ? "[DISCHARGING]" : "[IDLE]");
  if (pack.over_rating) n += snprintf(buf + n, sizeof(buf) - n, "  above rated %d mA", BQ76920_MAX_DISCHARGE_MA);
  snprintf(buf + n, sizeof(buf) - n, "\r\n");
  send(buf);
}

/* "Discharge stopped: <reason>" with the value that caused it. */
static void print_stop(DIS_Reason r)
{
  int n = snprintf(buf, sizeof(buf), "Discharge stopped: %s", DIS_ReasonText(r));
  if (r == DIS_CELL_UV)
  {
    n += snprintf(buf + n, sizeof(buf) - n, " (Cell%u = %u mV, limit %u mV)",
                  pack.culprit, pack.mv[pack.culprit - 1], DIS_CELL_CUTOFF_MV);
  }
  else if (r == DIS_PACK_UV)
  {
    n += snprintf(buf + n, sizeof(buf) - n, " (Pack = %lu mV, limit %u mV)",
                  (unsigned long)pack.pack_mv, DIS_PACK_CUTOFF_MV);
  }
  else if (r == DIS_OVERCURRENT)
  {
    n += snprintf(buf + n, sizeof(buf) - n, " (I = %ld mA, limit %d mA)", (long)pack.current_ma, DIS_OC_TRIP_MA);
  }
  else if (r == DIS_CHIP_FAULT)
  {
    n += snprintf(buf + n, sizeof(buf) - n, " (SYS_STAT=0x%02X)", pack.stat);
  }
  if (r != DIS_USER) n += snprintf(buf + n, sizeof(buf) - n, "\r\nSend \"discharge\" to start again");
  snprintf(buf + n, sizeof(buf) - n, "\r\n");
  send(buf);
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  if (BQ76920_Init(&hi2c1) != HAL_OK || DIS_Detect(&pack) != HAL_OK)
  {
    while (1)                                   /* repeat, so a late Hercules still sees it */
    {
      send("BMS init failed\r\n");
      HAL_Delay(1000);
    }
  }
  snprintf(buf, sizeof(buf), "%u cells found. Send \"discharge\" to start, \"stop discharge\" to stop\r\n", pack.count);
  send(buf);
  memset(rx_tail, ' ', sizeof(rx_tail) - 1);     /* 15 spaces + terminator */
  HAL_UART_Receive_IT(&huart2, &rx_byte, 1);    /* start listening; the callback re-arms it */
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    static uint32_t t_tick, t_print;            /* 250 ms protection tick, 1 s print */

    if (command != 0)
    {
      uint8_t c = command;
      command = 0;
      if (c == 1 && !pack.running)
      {
        DIS_Reason r = DIS_Start(&pack);
        if (r == DIS_NONE)
        {
          send("Discharge started\r\n");
        }
        else
        {
          pack.reason = r;
          print_stop(r);                        /* refused: the pack is already below a limit */
        }
      }
      else if (c == 2 && pack.running)
      {
        DIS_Stop(&pack, DIS_USER);
        print_stop(DIS_USER);
      }
    }

    if (HAL_GetTick() - t_tick >= 250)          /* the chip refreshes the current every 250 ms */
    {
      t_tick += 250;
      if (DIS_Tick(&pack)) print_stop(pack.reason);
    }
    if (HAL_GetTick() - t_print >= 1000)
    {
      t_print += 1000;
      print_status();
    }
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 180;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Activate the Over-Drive mode
  */
  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : BMS_ALERT_Pin */
  GPIO_InitStruct.Pin = BMS_ALERT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(BMS_ALERT_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
/* Called for every received byte. Match the end of the text, so no Enter is needed. */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *h)
{
  if (h->Instance == USART2)
  {
    if (rx_byte != '\r' && rx_byte != '\n')
    {
      memmove(rx_tail, rx_tail + 1, sizeof(rx_tail) - 2);      /* shift left, keep the last 14 characters */
      rx_tail[sizeof(rx_tail) - 2] = (char)tolower(rx_byte);
      rx_tail[sizeof(rx_tail) - 1] = 0;

      if (strstr(rx_tail, "stop discharge") != NULL)           /* test "stop discharge" first: it also ends in "discharge" */
      {
        command = 2;
        memset(rx_tail, ' ', sizeof(rx_tail) - 1);   /* back to spaces */
      }
      else if (strlen(rx_tail) >= 9 && strcmp(rx_tail + strlen(rx_tail) - 9, "discharge") == 0)
      {
        command = 1;
        memset(rx_tail, ' ', sizeof(rx_tail) - 1);   /* back to spaces */
      }
    }
    HAL_UART_Receive_IT(&huart2, &rx_byte, 1);                 /* re-arm, or no further byte arrives */
  }
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
