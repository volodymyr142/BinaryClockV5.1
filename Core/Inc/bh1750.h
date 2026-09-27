#ifndef BH1750_H
#define BH1750_H

#include "main.h"

/* Find and start the sensor. Returns 1 if it responded (address handled internally) */
uint8_t bh1750_init(void);

/* Raw 16-bit value from the sensor */
HAL_StatusTypeDef bh1750_read_raw(uint16_t *raw);

/* Convert the raw value to lux */
uint16_t bh1750_lux(uint16_t raw);

#endif /* BH1750_H */