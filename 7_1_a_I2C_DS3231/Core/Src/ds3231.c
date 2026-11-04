#include "ds3231.h"

#define DS3231_ADDR (0x68 << 1)
#define DS3231_REG_TIME 0x00
#define DS3231_TIMEOUT  100

static I2C_HandleTypeDef *ds3231_i2c;

static uint8_t bcd_to_dec(uint8_t v){
    return (v >> 4) *10 +(v & 0x0F);
}

HAL_StatusTypeDef DS3231_Init(I2C_HandleTypeDef *h2ic){
    ds3231_i2c = h2ic;
    return HAL_I2C_IsDeviceReady(h2ic, DS3231_ADDR, 3, 10);
}

HAL_StatusTypeDef DS3231_GetTime(DS3231_Time *time)
{
    uint8_t buf[7];
    HAL_StatusTypeDef st = HAL_I2C_Mem_Read(ds3231_i2c, DS3231_ADDR, DS3231_REG_TIME,
                                          I2C_MEMADD_SIZE_8BIT, buf, 7, DS3231_TIMEOUT);
    
    if (st != HAL_OK){
        return st;
    }
    time->second = bcd_to_dec(buf[0] & 0x7F);
    time->minute = bcd_to_dec(buf[1] & 0x7F);
    time->hour   = bcd_to_dec(buf[2] & 0x3F);   // bit 6 is the 12/24h flag
    time->day    = bcd_to_dec(buf[3] & 0x07);
    time->date   = bcd_to_dec(buf[4] & 0x3F);
    time->month  = bcd_to_dec(buf[5] & 0x1F);   // bit 7 is the century flag
    time->year   = bcd_to_dec(buf[6]);
    return HAL_OK;
}

static uint8_t dec_to_bcd(uint8_t v) { return ((v / 10) << 4) | (v % 10); }

HAL_StatusTypeDef DS3231_SetTime(const DS3231_Time *time)
{
  uint8_t buf[7] = {
    dec_to_bcd(time->second), dec_to_bcd(time->minute), dec_to_bcd(time->hour),
    dec_to_bcd(time->day),    dec_to_bcd(time->date),   dec_to_bcd(time->month),
    dec_to_bcd(time->year)
  };
  return HAL_I2C_Mem_Write(ds3231_i2c, DS3231_ADDR, DS3231_REG_TIME,
                           I2C_MEMADD_SIZE_8BIT, buf, 7, DS3231_TIMEOUT);
}

uint8_t DS3231_DayOfWeek(uint8_t date, uint8_t month, uint16_t year)
{
  static const uint8_t offset[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (month < 3) year -= 1;     // Jan and Feb count as months of the previous year
  return (year + year / 4 - year / 100 + year / 400 + offset[month - 1] + date) % 7 + 1;   // 1 = Sunday
}