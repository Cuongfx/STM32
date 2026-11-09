#include "discharge.h"

static uint8_t uv_count;                     /* consecutive samples under a cutoff */

static HAL_StatusTypeDef read_all(DIS_Pack *p)
{
    uint16_t raw[BQ76920_NUM_CELLS];

    if (BQ76920_ReadCells(raw) != HAL_OK) return HAL_ERROR;
    if (BQ76920_ReadCurrent(&p->current_ma) != HAL_OK) return HAL_ERROR;

    p->pack_mv = 0;
    for (uint8_t k = 0; k < p->count; k++)
    {
        p->mv[k] = raw[p->input[k]];
        p->pack_mv += p->mv[k];
    }
    return HAL_OK;
}

HAL_StatusTypeDef DIS_Detect(DIS_Pack *p)
{
    uint16_t raw[BQ76920_NUM_CELLS];

    if (BQ76920_ReadCells(raw) != HAL_OK) return HAL_ERROR;

    p->count = 0;
    for (uint8_t i = 0; i < BQ76920_NUM_CELLS; i++)
    {
        if (raw[i] >= DIS_CELL_PRESENT_MV)
        {
            p->input[p->count++] = i;        /* remember which chip input this cell is on */
        }
    }
    p->running = 0;
    p->reason  = DIS_NONE;
    if (p->count < 3) return HAL_ERROR;      /* the chip needs 3 cells at least */
    return read_all(p);
}

const char *DIS_ReasonText(DIS_Reason reason)
{
    switch (reason)
    {
    case DIS_USER:        return "stopped by user";
    case DIS_CELL_UV:     return "cell voltage below cutoff";
    case DIS_PACK_UV:     return "pack voltage below cutoff";
    case DIS_OVERCURRENT: return "overcurrent";
    case DIS_CHIP_FAULT:  return "protection of the bq76920 (OCD/SCD/UV/OV)";
    case DIS_I2C_ERROR:   return "I2C error";
    default:              return "none";
    }
}

void DIS_Stop(DIS_Pack *p, DIS_Reason reason)
{
    BQ76920_SetDischarge(0);                 /* FET off first, whatever else fails */
    p->running = 0;
    p->reason  = reason;
}

/* Which limit does the latest reading break? Sets culprit for a cell. */
static DIS_Reason check_limits(DIS_Pack *p)
{
    int32_t i = p->current_ma;
    if (i < 0) i = -i;
    p->over_rating = (i > BQ76920_MAX_DISCHARGE_MA) ? 1 : 0;
    if (i > DIS_OC_TRIP_MA) return DIS_OVERCURRENT;

    p->culprit = 0;
    for (uint8_t k = 0; k < p->count; k++)
    {
        if (p->mv[k] < DIS_CELL_CUTOFF_MV)
        {
            p->culprit = k + 1;
            return DIS_CELL_UV;
        }
    }
    if (p->pack_mv < DIS_PACK_CUTOFF_MV) return DIS_PACK_UV;
    return DIS_NONE;
}

DIS_Reason DIS_Start(DIS_Pack *p)
{
    if (read_all(p) != HAL_OK) return DIS_I2C_ERROR;

    DIS_Reason r = check_limits(p);          /* do not start a pack that is already below a cutoff */
    if (r == DIS_OVERCURRENT) r = DIS_NONE;  /* no load yet: a stale current value is not a reason */
    if (r != DIS_NONE) return r;

    BQ76920_ClearStatus(BQ_STAT_ALL);        /* forget every old flag (also CC_READY, OVRD_ALERT) */
    uv_count = 0;
    p->reason = DIS_NONE;
    if (BQ76920_SetDischarge(1) != HAL_OK) return DIS_I2C_ERROR;
    p->running = 1;
    return DIS_NONE;
}

uint8_t DIS_Tick(DIS_Pack *p)
{
    if (read_all(p) != HAL_OK)
    {
        if (p->running) { DIS_Stop(p, DIS_I2C_ERROR); return 1; }
        return 0;
    }
    if (!p->running) return 0;

    uint8_t stat = 0;
    if (BQ76920_ReadStatus(&stat) != HAL_OK)
    {
        DIS_Stop(p, DIS_I2C_ERROR);
        return 1;
    }
    if (stat & BQ_STAT_FAULTS)               /* the chip already cut the FET by itself */
    {
        p->stat = stat;
        DIS_Stop(p, DIS_CHIP_FAULT);
        return 1;
    }

    DIS_Reason r = check_limits(p);
    if (r == DIS_CELL_UV || r == DIS_PACK_UV)
    {
        if (++uv_count < DIS_UV_SAMPLES) return 0;   /* one noisy sample must not stop the discharge */
    }
    else
    {
        uv_count = 0;
    }
    if (r != DIS_NONE)
    {
        DIS_Stop(p, r);                      /* overcurrent stops at once */
        return 1;
    }
    return 0;
}
