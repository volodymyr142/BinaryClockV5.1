#ifndef SHIFT595_H
#define SHIFT595_H

#include "main.h"

/* Number of 74HC595 chips in the chain */
#define SR595_CHIP_COUNT   3u

/* Total number of outputs (bits) in the chain */
#define SR595_BIT_COUNT    (SR595_CHIP_COUNT * 8u)

void sr595_init(void);
void sr595_write(uint32_t frame);
/* Brightness 0..255 (255 = max). Drives /OE via TIM14 PWM. */
void sr595_set_brightness(uint8_t level);

#endif /* SHIFT595_H */