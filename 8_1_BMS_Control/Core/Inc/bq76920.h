#ifndef BQ76920_H
#define BQ76920_H

#include "stm32f4xx_hal.h"

#define BQ76920_NUM_CELLS 5U   /* the chip has 5 cell inputs, VC1..VC5 */

/* Keep the I2C handle, enable the ADC, read the factory gain and offset. */
HAL_StatusTypeDef BQ76920_Init(I2C_HandleTypeDef *hi2c);

/* Read all five cell inputs in one transfer. mv[0] = VC1 ... mv[4] = VC5, in mV. */
HAL_StatusTypeDef BQ76920_ReadCells(uint16_t mv[BQ76920_NUM_CELLS]);

/* Read one register (debugging). */
HAL_StatusTypeDef BQ76920_ReadReg(uint8_t reg, uint8_t *val);

#endif
