#include "bq76920.h"

#define BQ_ADDR           (0x08 << 1)   /* 7-bit address 0x08 in the 8-bit form HAL expects */

#define REG_CELLBAL1      0x01          /* bits 4..0 = CB5..CB1 */
#define REG_SYS_STAT      0x00
#define REG_SYS_CTRL1     0x04
#define REG_SYS_CTRL2     0x05
#define REG_CC_CFG        0x0B
#define REG_VC1_HI        0x0C
#define REG_CC_HI         0x32          /* signed 16-bit coulomb counter, 8.44 uV per step */
#define REG_ADCGAIN1      0x50
#define REG_ADCOFFSET     0x51
#define REG_ADCGAIN2      0x59

#define SYS_CTRL1_ADC_EN  0x10          /* bit 4: switch the ADC on */
#define SYS_CTRL2_CC_EN   0x40          /* bit 6: coulomb counter runs continuously (new value every 250 ms) */
#define SYS_CTRL2_DSG_ON  0x02          /* bit 1: discharge FET on */
#define CC_INVERT         1             /* 1: the chip reports charging as positive, so negate; set 0 if the sign is wrong */

static I2C_HandleTypeDef *bq_i2c;       /* handle kept from BQ76920_Init */
static int32_t gain_uv;                 /* ADC step in microvolts (about 365..396) */
static int8_t  offset_mv;               /* signed offset in millivolts */
static uint8_t sys_ctrl2;               /* copy of SYS_CTRL2 so DSG_ON can change without touching CC_EN */

HAL_StatusTypeDef BQ76920_ReadReg(uint8_t reg, uint8_t *val)
{
    return HAL_I2C_Mem_Read(bq_i2c, BQ_ADDR, reg, I2C_MEMADD_SIZE_8BIT, val, 1, 100);
}

static HAL_StatusTypeDef write_reg(uint8_t reg, uint8_t val)
{
    return HAL_I2C_Mem_Write(bq_i2c, BQ_ADDR, reg, I2C_MEMADD_SIZE_8BIT, &val, 1, 100);
}

HAL_StatusTypeDef BQ76920_SetBalance(uint8_t mask)
{
    return write_reg(REG_CELLBAL1, mask & 0x1F);   /* only CB1..CB5 exist */
}

HAL_StatusTypeDef BQ76920_Init(I2C_HandleTypeDef *hi2c)
{
    uint8_t gain1, gain2, off;

    bq_i2c = hi2c;

    if (HAL_I2C_IsDeviceReady(hi2c, BQ_ADDR, 3, 10) != HAL_OK) return HAL_ERROR;

    if (write_reg(REG_CC_CFG, 0x19) != HAL_OK) return HAL_ERROR;                 /* datasheet value */
    if (write_reg(REG_SYS_CTRL1, SYS_CTRL1_ADC_EN) != HAL_OK) return HAL_ERROR;  /* ADC on */

    if (BQ76920_ReadReg(REG_ADCGAIN1, &gain1) != HAL_OK) return HAL_ERROR;
    if (BQ76920_ReadReg(REG_ADCGAIN2, &gain2) != HAL_OK) return HAL_ERROR;
    if (BQ76920_ReadReg(REG_ADCOFFSET, &off) != HAL_OK) return HAL_ERROR;

    /* GAIN is a 5-bit number split over two registers: bits 3:2 of ADCGAIN1
       are its upper two bits, bits 7:5 of ADCGAIN2 are its lower three bits. */
    gain_uv   = 365 + ((gain1 & 0x0C) << 1) + ((gain2 & 0xE0) >> 5);
    offset_mv = (int8_t)off;

    sys_ctrl2 = SYS_CTRL2_CC_EN;        /* DSG stays off until the application asks for it */
    if (write_reg(REG_SYS_CTRL2, sys_ctrl2) != HAL_OK) return HAL_ERROR;

    HAL_Delay(300);                     /* the first conversion takes about 250 ms */
    return HAL_OK;
}

HAL_StatusTypeDef BQ76920_ReadCells(uint16_t mv[BQ76920_NUM_CELLS])
{
    uint8_t raw[2 * BQ76920_NUM_CELLS];

    /* One transfer of 10 bytes: the register address auto-increments,
       so VC1_HI, VC1_LO, VC2_HI, ... VC5_LO arrive back to back. */
    if (HAL_I2C_Mem_Read(bq_i2c, BQ_ADDR, REG_VC1_HI, I2C_MEMADD_SIZE_8BIT,
                         raw, sizeof(raw), 100) != HAL_OK)
    {
        return HAL_ERROR;
    }

    for (uint8_t i = 0; i < BQ76920_NUM_CELLS; i++)
    {
        uint16_t adc = ((raw[2 * i] & 0x3F) << 8) | raw[2 * i + 1];   /* 14-bit value */
        int32_t  v   = (int32_t)adc * gain_uv / 1000 + offset_mv;     /* uV -> mV, then offset */
        mv[i] = (v < 0) ? 0 : (uint16_t)v;
    }
    return HAL_OK;
}

HAL_StatusTypeDef BQ76920_ReadCurrent(int32_t *ma)
{
    uint8_t raw[2];

    if (HAL_I2C_Mem_Read(bq_i2c, BQ_ADDR, REG_CC_HI, I2C_MEMADD_SIZE_8BIT, raw, 2, 100) != HAL_OK)
    {
        return HAL_ERROR;
    }
    int16_t cc = (int16_t)((raw[0] << 8) | raw[1]);          /* signed: 8.44 uV per step */
    int32_t i  = (int32_t)cc * 8440 / (int32_t)BQ76920_RSENSE_UOHM;   /* nV / uOhm = mA */
    *ma = CC_INVERT ? -i : i;
    return HAL_OK;
}

HAL_StatusTypeDef BQ76920_SetDischarge(uint8_t on)
{
    if (on) sys_ctrl2 |= SYS_CTRL2_DSG_ON;
    else    sys_ctrl2 &= (uint8_t)~SYS_CTRL2_DSG_ON;
    return write_reg(REG_SYS_CTRL2, sys_ctrl2);
}

HAL_StatusTypeDef BQ76920_ReadStatus(uint8_t *stat)
{
    return BQ76920_ReadReg(REG_SYS_STAT, stat);
}

HAL_StatusTypeDef BQ76920_ClearStatus(uint8_t mask)
{
    return write_reg(REG_SYS_STAT, mask);                    /* writing 1 clears a flag */
}
