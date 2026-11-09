#ifndef DISCHARGE_H
#define DISCHARGE_H

#include "bq76920.h"

#define DIS_CELL_PRESENT_MV  1500U   /* a chip input below this is not a cell */
#define DIS_CELL_CUTOFF_MV   3000U   /* stop when any cell is below this */
#define DIS_PACK_CUTOFF_MV   9000U   /* stop when the pack total is below this */
#define DIS_OC_TRIP_MA       5000    /* stop when the pack current is above this (any direction) */
#define DIS_UV_SAMPLES       2U      /* undervoltage must hold for this many 250 ms samples */

typedef enum {
    DIS_NONE = 0,        /* no fault */
    DIS_USER,            /* user sent "stop discharge" */
    DIS_CELL_UV,         /* one cell under DIS_CELL_CUTOFF_MV */
    DIS_PACK_UV,         /* pack under DIS_PACK_CUTOFF_MV */
    DIS_OVERCURRENT,     /* current over DIS_OC_TRIP_MA */
    DIS_CHIP_FAULT,      /* the bq76920 switched DSG off itself (OCD, SCD, UV, OV) */
    DIS_I2C_ERROR        /* communication with the chip failed */
} DIS_Reason;

typedef struct {
    uint8_t    count;                        /* cells found (3 to 5) */
    uint8_t    input[BQ76920_NUM_CELLS];     /* chip input each cell is wired to */
    uint16_t   mv[BQ76920_NUM_CELLS];        /* cell voltages in mV */
    uint32_t   pack_mv;                      /* sum of the cells in mV */
    int32_t    current_ma;                   /* pack current, positive = discharging */
    uint8_t    running;                      /* 1 while the discharge FET is on */
    DIS_Reason reason;                       /* why the last discharge stopped */
    uint8_t    culprit;                      /* cell number (1..) behind DIS_CELL_UV, 0 otherwise */
    uint8_t    stat;                         /* SYS_STAT when reason == DIS_CHIP_FAULT */
    uint8_t    over_rating;                  /* 1 while current is above BQ76920_MAX_DISCHARGE_MA */
} DIS_Pack;

/* Find the cells and read everything once. Call before the first DIS_Start. */
HAL_StatusTypeDef DIS_Detect(DIS_Pack *p);

/* Check the pack and switch the discharge FET on. Returns the reason it refused, or DIS_NONE. */
DIS_Reason DIS_Start(DIS_Pack *p);

/* Switch the discharge FET off. reason is stored in p->reason. */
void DIS_Stop(DIS_Pack *p, DIS_Reason reason);

/* Call every 250 ms: read cells and current, apply every limit, stop the discharge when one is hit.
   Returns 1 when this call stopped the discharge, otherwise 0. */
uint8_t DIS_Tick(DIS_Pack *p);

/* Text for a stop reason. */
const char *DIS_ReasonText(DIS_Reason reason);

#endif
