#ifndef DS3231_H
#define DS3231_H

#include "main.h"
#include "clocktime.h"

/* Bus address: 0x68 in 7-bit form.
   HAL expects 8-bit, so shift left by 1. */
#define DS3231_I2C_ADDR   (0x68u << 1)

/* Date. Year is stored as 0..99, real year = 2000 + year */
typedef struct
{
    uint8_t day;      /* 1..31 */
    uint8_t month;    /* 1..12 */
    uint8_t year;     /* 0..99  */
} clock_date_t;

HAL_StatusTypeDef ds3231_read_time(clock_time_t *out);
HAL_StatusTypeDef ds3231_write_time(const clock_time_t *in);

HAL_StatusTypeDef ds3231_read_date(clock_date_t *out);
HAL_StatusTypeDef ds3231_write_date(const clock_date_t *in);

/* 1 if the oscillator stopped and the stored time can't be trusted */
uint8_t ds3231_lost_power(void);

/* Alarm (Alarm1), fires daily at hours:minutes:00 */
HAL_StatusTypeDef ds3231_set_alarm(uint8_t hours, uint8_t minutes);
HAL_StatusTypeDef ds3231_get_alarm(uint8_t *hours, uint8_t *minutes);
HAL_StatusTypeDef ds3231_alarm_enable(uint8_t on);
uint8_t           ds3231_alarm_is_enabled(void);
HAL_StatusTypeDef ds3231_alarm_clear(void);

#endif /* DS3231_H */