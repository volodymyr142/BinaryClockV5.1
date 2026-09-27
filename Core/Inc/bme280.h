#ifndef BME280_H
#define BME280_H

#include "main.h"

typedef struct
{
    int16_t  temp_c100;    /* temperature in hundredths of °C: 2345 = 23.45 °C */
    uint8_t  humidity;     /* humidity, % (0..100) */
    uint32_t pressure_pa;  /* pressure in pascals */
} bme280_data_t;

uint8_t bme280_init(void);                    /* 1 if found */
HAL_StatusTypeDef bme280_read(bme280_data_t *out);

#endif /* BME280_H */