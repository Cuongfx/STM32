#ifndef DS3231_H
#define DS3231_H

#include "main.h"

typedef struct {
  uint8_t second, minute, hour;
  uint8_t day;                   // weekday, 1 = Sunday
  uint8_t date, month, year;     // year 0..99 means 2000 + year
} DS3231_Time;

uint8_t DS3231_DayOfWeek(uint8_t date, uint8_t month, uint16_t year); 

HAL_StatusTypeDef DS3231_Init(I2C_HandleTypeDef *hi2c);
HAL_StatusTypeDef DS3231_GetTime(DS3231_Time *time);
HAL_StatusTypeDef DS3231_SetTime(const DS3231_Time *time);

#endif