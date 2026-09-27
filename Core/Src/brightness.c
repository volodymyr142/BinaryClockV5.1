#include "brightness.h"
#include "bh1750.h"
#include "shift595.h"

#define BR_PERIOD_MS   80u     /* update period, ms */
#define BR_STEP        16u      /* max brightness change per step (smoothing) */
#define BR_MIN         8u      /* minimum, so it doesn't go fully dark at night */
#define BR_MAX         255u    /* maximum */
#define BR_LUX_FULL    150u    /* at this illuminance (lux) — full brightness */

static uint32_t next_tick = 0u;
static uint16_t current   = 128u;   /* uint16 so += BR_STEP doesn't overflow 255 */

void brightness_init(void)
{
    current   = 128u;
    next_tick = HAL_GetTick() + BR_PERIOD_MS;
    sr595_set_brightness((uint8_t)current);
}

void brightness_task(void)
{
    if ((int32_t)(HAL_GetTick() - next_tick) < 0)
    {
        return;
    }
    next_tick += BR_PERIOD_MS;

    uint16_t raw = 0u;
    if (bh1750_read_raw(&raw) != HAL_OK)
    {
        return;                       /* sensor didn't respond -> leave as is */
    }

    uint16_t lux = bh1750_lux(raw);

    /* Linear: 0..BR_LUX_FULL lux -> BR_MIN..BR_MAX brightness */
    uint16_t target;
    if (lux >= BR_LUX_FULL)
    {
        target = BR_MAX;
    }
    else
    {
        target = (uint16_t)(BR_MIN + ((uint32_t)lux * (BR_MAX - BR_MIN)) / BR_LUX_FULL);
    }

    /* Smoothly move toward the target by no more than BR_STEP per step */
    if (current < target)
    {
        current += BR_STEP;
        if (current > target) { current = target; }
    }
    else if (current > target)
    {
        if (current < target + BR_STEP) { current = target; }
        else                            { current -= BR_STEP; }
    }

    sr595_set_brightness((uint8_t)current);
}