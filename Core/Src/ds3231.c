#include "ds3231.h"

/* The I2C handle is created by MX_I2C1_Init() in main.c.
   extern tells the compiler: the variable exists, it's defined elsewhere. */
extern I2C_HandleTypeDef hi2c1;

#define DS3231_REG_SECONDS   0x00u
#define DS3231_REG_STATUS    0x0Fu
#define DS3231_TIMEOUT_MS    100u
#define DS3231_REG_DATE      0x04u
#define DS3231_REG_ALARM1    0x07u
#define DS3231_REG_CONTROL   0x0Eu

/* ---- BCD conversion ---- */

static uint8_t bcd_to_bin(uint8_t bcd)
{
    return (uint8_t)(((bcd >> 4) * 10u) + (bcd & 0x0Fu));
}

static uint8_t bin_to_bcd(uint8_t bin)
{
    return (uint8_t)(((bin / 10u) << 4) | (bin % 10u));
}

/* ---- Reading time ---- */

HAL_StatusTypeDef ds3231_read_time(clock_time_t *out)
{
    uint8_t raw[3];
    HAL_StatusTypeDef st;

    st = HAL_I2C_Mem_Read(&hi2c1, DS3231_I2C_ADDR,
                          DS3231_REG_SECONDS, I2C_MEMADD_SIZE_8BIT,
                          raw, sizeof(raw), DS3231_TIMEOUT_MS);
    if (st != HAL_OK)
    {
        return st;
    }

    out->seconds = bcd_to_bin(raw[0] & 0x7Fu);
    out->minutes = bcd_to_bin(raw[1] & 0x7Fu);

    if (raw[2] & 0x40u)
    {
        /* 12-hour mode: bit 5 = PM, bits 4:0 = hours 1..12 */
        uint8_t pm  = (raw[2] & 0x20u) ? 1u : 0u;
        uint8_t h12 = bcd_to_bin(raw[2] & 0x1Fu);

        if (h12 == 12u)          /* 12 AM = 0, 12 PM = 12 */
        {
            h12 = 0u;
        }
        out->hours = (uint8_t)(h12 + (pm ? 12u : 0u));
    }
    else
    {
        /* 24-hour mode: bits 5:0 are BCD (bit 5 = tens, the 20s digit) */
        out->hours = bcd_to_bin(raw[2] & 0x3Fu);
    }

    return HAL_OK;
}

/* ---- Writing time ---- */

HAL_StatusTypeDef ds3231_write_time(const clock_time_t *in)
{
    uint8_t raw[3];
    uint8_t status;
    HAL_StatusTypeDef st;

    raw[0] = bin_to_bcd((uint8_t)(in->seconds % 60u));
    raw[1] = bin_to_bcd((uint8_t)(in->minutes % 60u));
    raw[2] = bin_to_bcd((uint8_t)(in->hours   % 24u));  /* bit 6 = 0 -> 24-hour mode */

    st = HAL_I2C_Mem_Write(&hi2c1, DS3231_I2C_ADDR,
                           DS3231_REG_SECONDS, I2C_MEMADD_SIZE_8BIT,
                           raw, sizeof(raw), DS3231_TIMEOUT_MS);
    if (st != HAL_OK)
    {
        return st;
    }

    /* Clear the OSF flag: the time was just set, it's trustworthy now */
    st = HAL_I2C_Mem_Read(&hi2c1, DS3231_I2C_ADDR,
                          DS3231_REG_STATUS, I2C_MEMADD_SIZE_8BIT,
                          &status, 1u, DS3231_TIMEOUT_MS);
    if (st != HAL_OK)
    {
        return st;
    }

    status &= (uint8_t)~0x80u;

    return HAL_I2C_Mem_Write(&hi2c1, DS3231_I2C_ADDR,
                             DS3231_REG_STATUS, I2C_MEMADD_SIZE_8BIT,
                             &status, 1u, DS3231_TIMEOUT_MS);
}

HAL_StatusTypeDef ds3231_read_date(clock_date_t *out)
{
    uint8_t raw[3];
    HAL_StatusTypeDef st;

    st = HAL_I2C_Mem_Read(&hi2c1, DS3231_I2C_ADDR,
                          DS3231_REG_DATE, I2C_MEMADD_SIZE_8BIT,
                          raw, sizeof(raw), DS3231_TIMEOUT_MS);
    if (st != HAL_OK)
    {
        return st;
    }

    out->day   = bcd_to_bin(raw[0] & 0x3Fu);
    out->month = bcd_to_bin(raw[1] & 0x1Fu);   /* bit 7 = century, mask it off */
    out->year  = bcd_to_bin(raw[2]);

    return HAL_OK;
}

HAL_StatusTypeDef ds3231_write_date(const clock_date_t *in)
{
    uint8_t raw[3];

    raw[0] = bin_to_bcd((uint8_t)(in->day));
    raw[1] = bin_to_bcd((uint8_t)(in->month));   /* bit 7 = 0 -> century 20xx */
    raw[2] = bin_to_bcd((uint8_t)(in->year));

    return HAL_I2C_Mem_Write(&hi2c1, DS3231_I2C_ADDR,
                             DS3231_REG_DATE, I2C_MEMADD_SIZE_8BIT,
                             raw, sizeof(raw), DS3231_TIMEOUT_MS);
}

/* ---- Whether the oscillator stopped ---- */

uint8_t ds3231_lost_power(void)
{
    uint8_t status = 0u;

    if (HAL_I2C_Mem_Read(&hi2c1, DS3231_I2C_ADDR,
                         DS3231_REG_STATUS, I2C_MEMADD_SIZE_8BIT,
                         &status, 1u, DS3231_TIMEOUT_MS) != HAL_OK)
    {
        return 1u;    /* no communication — assume the time can't be trusted */
    }

    return (status & 0x80u) ? 1u : 0u;
}

/* ---- Alarm (Alarm1) ---- */

HAL_StatusTypeDef ds3231_set_alarm(uint8_t hours, uint8_t minutes)
{
    uint8_t a[4];
    a[0] = bin_to_bcd(0u);                        /* seconds=0, A1M1=0 */
    a[1] = bin_to_bcd((uint8_t)(minutes % 60u));  /* A1M2=0 */
    a[2] = bin_to_bcd((uint8_t)(hours % 24u));    /* A1M3=0, 24-hour */
    a[3] = 0x80u;                                 /* A1M4=1 -> day is ignored */

    return HAL_I2C_Mem_Write(&hi2c1, DS3231_I2C_ADDR, DS3231_REG_ALARM1,
                             I2C_MEMADD_SIZE_8BIT, a, 4u, DS3231_TIMEOUT_MS);
}

HAL_StatusTypeDef ds3231_get_alarm(uint8_t *hours, uint8_t *minutes)
{
    uint8_t a[3];
    HAL_StatusTypeDef st = HAL_I2C_Mem_Read(&hi2c1, DS3231_I2C_ADDR,
                                            DS3231_REG_ALARM1, I2C_MEMADD_SIZE_8BIT,
                                            a, 3u, DS3231_TIMEOUT_MS);
    if (st != HAL_OK) { return st; }

    *minutes = bcd_to_bin(a[1] & 0x7Fu);
    *hours   = bcd_to_bin(a[2] & 0x3Fu);
    return HAL_OK;
}

HAL_StatusTypeDef ds3231_alarm_clear(void)
{
    uint8_t reg = 0u;
    HAL_StatusTypeDef st = HAL_I2C_Mem_Read(&hi2c1, DS3231_I2C_ADDR,
                                            DS3231_REG_STATUS, I2C_MEMADD_SIZE_8BIT,
                                            &reg, 1u, DS3231_TIMEOUT_MS);
    if (st != HAL_OK) { return st; }

    reg &= (uint8_t)~0x01u;           /* clear A1F */
    return HAL_I2C_Mem_Write(&hi2c1, DS3231_I2C_ADDR, DS3231_REG_STATUS,
                             I2C_MEMADD_SIZE_8BIT, &reg, 1u, DS3231_TIMEOUT_MS);
}

HAL_StatusTypeDef ds3231_alarm_enable(uint8_t on)
{
    uint8_t reg;
    HAL_StatusTypeDef st;

    /* bit A1IE (0) + INTCN (2): enable the alarm and route INT to the SQW pin */
    st = HAL_I2C_Mem_Read(&hi2c1, DS3231_I2C_ADDR, DS3231_REG_CONTROL,
                          I2C_MEMADD_SIZE_8BIT, &reg, 1u, DS3231_TIMEOUT_MS);
    if (st != HAL_OK) { return st; }

    if (on) { reg |= (uint8_t)(0x01u | 0x04u); }
    else    { reg &= (uint8_t)~0x01u; }

    st = HAL_I2C_Mem_Write(&hi2c1, DS3231_I2C_ADDR, DS3231_REG_CONTROL,
                           I2C_MEMADD_SIZE_8BIT, &reg, 1u, DS3231_TIMEOUT_MS);
    if (st != HAL_OK) { return st; }

    return ds3231_alarm_clear();      /* clear any stale flag left over */
}

uint8_t ds3231_alarm_is_enabled(void)
{
    uint8_t reg = 0u;
    if (HAL_I2C_Mem_Read(&hi2c1, DS3231_I2C_ADDR, DS3231_REG_CONTROL,
                         I2C_MEMADD_SIZE_8BIT, &reg, 1u, DS3231_TIMEOUT_MS) != HAL_OK)
    {
        return 0u;
    }
    return (reg & 0x01u) ? 1u : 0u;
}