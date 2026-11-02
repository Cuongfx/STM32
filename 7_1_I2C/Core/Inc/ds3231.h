#ifndef DS3231_H
#define DS3231_H

#include "main.h"   // HAL types: I2C_HandleTypeDef, HAL_StatusTypeDef

typedef struct
{
  uint8_t second;   // 0..59
  uint8_t minute;   // 0..59
  uint8_t hour;     // 0..23
  uint8_t day;      // day of week, 1 = Sunday .. 7 = Saturday
  uint8_t date;     // day of month, 1..31
  uint8_t month;    // 1..12
  uint8_t year;     // 0..99 (2000 + year)
} DS3231_Time;

HAL_StatusTypeDef DS3231_Init(I2C_HandleTypeDef *hi2c);
HAL_StatusTypeDef DS3231_SetTime(const DS3231_Time *time);
HAL_StatusTypeDef DS3231_GetTime(DS3231_Time *time);
uint8_t           DS3231_DayOfWeek(uint8_t date, uint8_t month, uint16_t year);

#endif