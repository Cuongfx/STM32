#ifndef BQ76920_H
#define BQ76920_H

#include "stm32f4xx_hal.h"

#define BQ76920_NUM_CELLS     5U      /* the chip has 5 cell inputs, VC1..VC5 */
#define BQ76920_RSENSE_UOHM   1000U   /* sense resistor R11 on the EVM: 1 mOhm */
#define BQ76920_MAX_DISCHARGE_MA 4000 /* rated discharge current of this BMS setup; change if needed */

/* SYS_STAT flags (register 0x00); write 1 to a bit to clear it. */
#define BQ_STAT_OCD   0x01U           /* chip switched DSG off: overcurrent in discharge */
#define BQ_STAT_SCD   0x02U           /* chip switched DSG off: short circuit */
#define BQ_STAT_OV    0x04U           /* a cell is over the OV trip level */
#define BQ_STAT_UV    0x08U           /* a cell is under the UV trip level (DSG off) */
#define BQ_STAT_FAULTS (BQ_STAT_OCD | BQ_STAT_SCD | BQ_STAT_OV | BQ_STAT_UV)
#define BQ_STAT_ALL    0xBFU          /* every clearable flag (bit 6 is reserved) */

/* Keep the I2C handle, enable the ADC and the coulomb counter, read the factory gain and offset. */
HAL_StatusTypeDef BQ76920_Init(I2C_HandleTypeDef *hi2c);

/* Read all five cell inputs in one transfer. mv[0] = VC1 ... mv[4] = VC5, in mV. */
HAL_StatusTypeDef BQ76920_ReadCells(uint16_t mv[BQ76920_NUM_CELLS]);

/* Pack current in mA, positive while discharging (new value every 250 ms). */
HAL_StatusTypeDef BQ76920_ReadCurrent(int32_t *ma);

/* Switch the discharge FET (DSG_ON bit of SYS_CTRL2); the coulomb counter stays on. */
HAL_StatusTypeDef BQ76920_SetDischarge(uint8_t on);

/* Read SYS_STAT and clear the flags in mask (0 = only read). */
HAL_StatusTypeDef BQ76920_ReadStatus(uint8_t *stat);
HAL_StatusTypeDef BQ76920_ClearStatus(uint8_t mask);

/* Switch the internal bleeders. Bit n of mask = chip input VC(n+1). 0 switches every bleeder off. */
HAL_StatusTypeDef BQ76920_SetBalance(uint8_t mask);

/* Read one register (debugging). */
HAL_StatusTypeDef BQ76920_ReadReg(uint8_t reg, uint8_t *val);

#endif
