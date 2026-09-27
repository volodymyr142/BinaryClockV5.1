#include "clocktime.h"

static clock_time_t now       = { 0u, 0u, 0u };
static uint32_t     next_tick = 0u;

void clock_time_init(uint8_t hours, uint8_t minutes, uint8_t seconds)
{
    now.hours   = hours   % 24u;
    now.minutes = minutes % 60u;
    now.seconds = seconds % 60u;

    next_tick = HAL_GetTick() + 1000u;
}

uint8_t clock_time_update(void)
{
    /* Has the next second arrived yet? */
    if ((int32_t)(HAL_GetTick() - next_tick) < 0)
    {
        return 0u;
    }

    /* Advance exactly one second from the PREVIOUS deadline, not from now —
       otherwise the error accumulates. */
    next_tick += 1000u;

    if (++now.seconds >= 60u)
    {
        now.seconds = 0u;

        if (++now.minutes >= 60u)
        {
            now.minutes = 0u;

            if (++now.hours >= 24u)
            {
                now.hours = 0u;
            }
        }
    }

    return 1u;
}

const clock_time_t *clock_time_get(void)
{
    return &now;
}