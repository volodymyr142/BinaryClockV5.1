#ifndef BUZZER_H
#define BUZZER_H

#include "main.h"

void buzzer_init(void);
void buzzer_task(void);   /* call every iteration */

/* Beep count times (on_ms tone, off_ms pause).
   count = 0 -> continuous until buzzer_stop() (for the alarm) */
void buzzer_beep(uint16_t count, uint16_t on_ms, uint16_t off_ms);
void buzzer_stop(void);
uint8_t buzzer_is_active(void);

#endif /* BUZZER_H */