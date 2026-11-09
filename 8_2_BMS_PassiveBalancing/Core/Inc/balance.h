#ifndef BALANCE_H
#define BALANCE_H

#include "bq76920.h"

#define BAL_CELL_PRESENT_MV  1500U  /* a chip input below this is not a cell */
#define BAL_FINISH_MV        20U      /* balancing is done when highest - lowest <= this */
#define BAL_BLEED_SECONDS    4U      /* bleed for 4 s, then rest for 1 s and measure */

typedef enum { BAL_RUNNING, BAL_DONE, BAL_ERROR } BAL_State;

typedef struct {
    uint8_t  count;                          /* cells found (3 to 5) */
    uint8_t  input[BQ76920_NUM_CELLS];       /* chip input (0..4) each cell is wired to */
    uint16_t mv[BQ76920_NUM_CELLS];          /* voltage of each cell in mV, same order */
    uint8_t  bleed;                          /* bleeders on now (bit n = chip input n) */
} BAL_Pack;

/* Read the five inputs once and find which of them are cells. */
HAL_StatusTypeDef BAL_Detect(BAL_Pack *p);

/* Read the voltage of the known cells (no bleeding change). */
HAL_StatusTypeDef BAL_Read(BAL_Pack *p);

/* Call once per second: 4 s bleeding, 1 s rest and measure, until the pack is balanced. */
BAL_State BAL_Tick(BAL_Pack *p);

#endif
