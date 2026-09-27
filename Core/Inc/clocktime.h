#ifndef CLOCKTIME_H
#define CLOCKTIME_H

#include "main.h"

/* Current time. Seconds aren't displayed yet (no LEDs for them),
   but are needed for counting and useful for blinking a separator. */
typedef struct
{
    uint8_t hours;      /* 0..23 */
    uint8_t minutes;    /* 0..59 */
    uint8_t seconds;    /* 0..59 */
} clock_time_t;

void clock_time_init(uint8_t hours, uint8_t minutes, uint8_t seconds);

/* Call as often as possible from the main loop.
   Returns 1 if the second changed (i.e. the display needs updating). */
uint8_t clock_time_update(void);

const clock_time_t *clock_time_get(void);

#endif /* CLOCKTIME_H */