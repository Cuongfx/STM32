#include "ds3231.h"

#define DS3231_ADDR      (0x68 << 1)   // HAL wants the 7-bit address shifted left by 1
#define DS3231_REG_TIME  0x00          // first time register (seconds)
#define DS3231_TIMEOUT   100U          // ms

static I2C_HandleTypeDef *ds3231_i2c;  // set by DS3231_Init, used by every function

static uint8_t dec_to_bcd(uint8_t value)
{
  return (uint8_t)(((value / 10) << 4) | (value % 10));
}

static uint8_t bcd_to_dec(uint8_t value)
{
  return (uint8_t)(((value >> 4) * 10) + (value & 0x0F));
}

// Sakamoto's Algorithm
uint8_t DS3231_DayOfWeek(uint8_t date, uint8_t month, uint16_t year)
{
  static const uint8_t offset[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};

  if (month < 3)
  {
    year -= 1;   // Jan and Feb count as part of the previous year
  }

  uint8_t w = (uint8_t)((year + year / 4 - year / 100 + year / 400
                         + offset[month - 1] + date) % 7);   // 0 = Sunday
  return (uint8_t)(w + 1);                                      // DS3231 stores 1..7
}

HAL_StatusTypeDef DS3231_Init(I2C_HandleTypeDef *hi2c)
{
  ds3231_i2c = hi2c;

  // Check that a device answers at this address (3 tries, 10ms each)
  return HAL_I2C_IsDeviceReady(ds3231_i2c, DS3231_ADDR, 3, 10);
}

HAL_StatusTypeDef DS3231_SetTime(const DS3231_Time *time)
{
  uint8_t buf[7];

  buf[0] = dec_to_bcd(time->second);
  buf[1] = dec_to_bcd(time->minute);
  buf[2] = dec_to_bcd(time->hour);     // bit 6 = 0 keeps 24h mode
  buf[3] = dec_to_bcd(time->day);
  buf[4] = dec_to_bcd(time->date);
  buf[5] = dec_to_bcd(time->month);
  buf[6] = dec_to_bcd(time->year);

  return HAL_I2C_Mem_Write(ds3231_i2c, DS3231_ADDR, DS3231_REG_TIME,
                           I2C_MEMADD_SIZE_8BIT, buf, 7, DS3231_TIMEOUT);
}

HAL_StatusTypeDef DS3231_GetTime(DS3231_Time *time)
{
  uint8_t buf[7];

  HAL_StatusTypeDef status = HAL_I2C_Mem_Read(ds3231_i2c, DS3231_ADDR, DS3231_REG_TIME,
                                              I2C_MEMADD_SIZE_8BIT, buf, 7, DS3231_TIMEOUT);
  if (status != HAL_OK)
  {
    return status;   // leave *time untouched on failure
  }

  time->second = bcd_to_dec(buf[0] & 0x7F);
  time->minute = bcd_to_dec(buf[1] & 0x7F);
  time->hour   = bcd_to_dec(buf[2] & 0x3F);   // mask the 12/24h flag bits
  time->day    = bcd_to_dec(buf[3] & 0x07);
  time->date   = bcd_to_dec(buf[4] & 0x3F);
  time->month  = bcd_to_dec(buf[5] & 0x1F);   // bit 7 is the century flag
  time->year   = bcd_to_dec(buf[6]);

  return HAL_OK;
}