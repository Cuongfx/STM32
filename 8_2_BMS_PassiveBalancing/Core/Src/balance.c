#include "balance.h"

static uint8_t phase = BAL_BLEED_SECONDS;   /* 0..3 = bleeding, 4 = rest and measure; see BAL_Tick */

HAL_StatusTypeDef BAL_Detect(BAL_Pack *p)
{
    uint16_t raw[BQ76920_NUM_CELLS];

    if (BQ76920_ReadCells(raw) != HAL_OK) return HAL_ERROR;

    p->count = 0;
    p->bleed = 0;
    for (uint8_t i = 0; i < BQ76920_NUM_CELLS; i++)
    {
        if (raw[i] >= BAL_CELL_PRESENT_MV)
        {
            p->input[p->count] = i;          /* remember which chip input this cell is on */
            p->mv[p->count]    = raw[i];
            p->count++;
        }
    }
    phase = BAL_BLEED_SECONDS;               /* the first tick measures, bleeders are off */
    return (p->count >= 3) ? HAL_OK : HAL_ERROR;   /* the chip needs 3 cells at least */
}

HAL_StatusTypeDef BAL_Read(BAL_Pack *p)
{
    uint16_t raw[BQ76920_NUM_CELLS];

    if (BQ76920_ReadCells(raw) != HAL_OK) return HAL_ERROR;
    for (uint8_t k = 0; k < p->count; k++)
    {
        p->mv[k] = raw[p->input[k]];
    }
    return HAL_OK;
}

/* Pick the cells to bleed: highest first, only cells more than BAL_FINISH_MV above the lowest,
   and never a cell whose neighbouring chip input is already bleeding. */
static uint8_t choose_bleed(const BAL_Pack *p, uint16_t min_mv)
{
    uint8_t mask = 0, used = 0;

    for (;;)
    {
        int best = -1;                       /* highest cell not looked at yet */
        for (uint8_t i = 0; i < p->count; i++)
        {
            if ((used & (1U << i)) == 0 && p->mv[i] > min_mv + BAL_FINISH_MV &&
                (best < 0 || p->mv[i] > p->mv[best]))
            {
                best = i;
            }
        }
        if (best < 0) break;

        used |= 1U << best;
        uint8_t in = p->input[best];
        uint8_t neighbours = ((1U << in) << 1) | ((1U << in) >> 1);
        if ((mask & neighbours) == 0)
        {
            mask |= 1U << in;
        }
    }
    return mask;
}

BAL_State BAL_Tick(BAL_Pack *p)
{
    if (BAL_Read(p) != HAL_OK)
    {
        BQ76920_SetBalance(0);               /* never leave the bleeders on after an error */
        p->bleed = 0;
        return BAL_ERROR;
    }

    if (phase == BAL_BLEED_SECONDS)          /* bleeders were off for a second: accurate reading */
    {
        uint16_t lo = p->mv[0], hi = p->mv[0];
        for (uint8_t k = 1; k < p->count; k++)
        {
            if (p->mv[k] < lo) lo = p->mv[k];
            if (p->mv[k] > hi) hi = p->mv[k];
        }

        if ((unsigned)(hi - lo) <= BAL_FINISH_MV)   /* hi >= lo, so the difference is not negative */
        {
            p->bleed = 0;
            BQ76920_SetBalance(0);
            return BAL_DONE;
        }
        p->bleed = choose_bleed(p, lo);
        if (BQ76920_SetBalance(p->bleed) != HAL_OK)
        {
            p->bleed = 0;
            BQ76920_SetBalance(0);
            return BAL_ERROR;
        }
        phase = 0;                           /* bleeding ticks 0..3 = 4 s, then 1 s rest */
    }
    else if (phase == BAL_BLEED_SECONDS - 1) /* last bleeding second: switch off for the next tick */
    {
        p->bleed = 0;
        BQ76920_SetBalance(0);
        phase = BAL_BLEED_SECONDS;
    }
    else
    {
        phase++;
    }
    return BAL_RUNNING;
}
