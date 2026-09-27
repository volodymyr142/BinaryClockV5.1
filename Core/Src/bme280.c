#include "bme280.h"

extern I2C_HandleTypeDef hi2c1;

#define BME280_TIMEOUT_MS   100u

/* Registers */
#define BME280_REG_ID        0xD0u
#define BME280_REG_CTRL_HUM  0xF2u
#define BME280_REG_CTRL_MEAS 0xF4u
#define BME280_REG_CONFIG    0xF5u
#define BME280_REG_DATA      0xF7u   /* 8 bytes: pressure, temp, humidity */
#define BME280_REG_CALIB00   0x88u   /* 26 bytes */
#define BME280_REG_CALIB26   0xE1u   /* 7 bytes  */

/* 8-bit address (already shifted). 0 = not found */
static uint8_t bme_addr = 0u;

/* Calibration coefficients (named as in the Bosch datasheet) */
static uint16_t dig_T1;
static int16_t  dig_T2, dig_T3;
static uint16_t dig_P1;
static int16_t  dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
static uint8_t  dig_H1, dig_H3;
static int16_t  dig_H2, dig_H4, dig_H5;
static int8_t   dig_H6;

/* Shared intermediate value: temperature feeds into pressure and humidity */
static int32_t t_fine;

static HAL_StatusTypeDef bme_read_regs(uint8_t reg, uint8_t *buf, uint16_t len)
{
    return HAL_I2C_Mem_Read(&hi2c1, bme_addr, reg, I2C_MEMADD_SIZE_8BIT,
                            buf, len, BME280_TIMEOUT_MS);
}

static HAL_StatusTypeDef bme_write_reg(uint8_t reg, uint8_t val)
{
    return HAL_I2C_Mem_Write(&hi2c1, bme_addr, reg, I2C_MEMADD_SIZE_8BIT,
                             &val, 1u, BME280_TIMEOUT_MS);
}

static void bme_read_calibration(void)
{
    uint8_t c[26];
    uint8_t h[7];

    bme_read_regs(BME280_REG_CALIB00, c, 26u);
    bme_read_regs(BME280_REG_CALIB26, h, 7u);

    dig_T1 = (uint16_t)((c[1]  << 8) | c[0]);
    dig_T2 = (int16_t) ((c[3]  << 8) | c[2]);
    dig_T3 = (int16_t) ((c[5]  << 8) | c[4]);
    dig_P1 = (uint16_t)((c[7]  << 8) | c[6]);
    dig_P2 = (int16_t) ((c[9]  << 8) | c[8]);
    dig_P3 = (int16_t) ((c[11] << 8) | c[10]);
    dig_P4 = (int16_t) ((c[13] << 8) | c[12]);
    dig_P5 = (int16_t) ((c[15] << 8) | c[14]);
    dig_P6 = (int16_t) ((c[17] << 8) | c[16]);
    dig_P7 = (int16_t) ((c[19] << 8) | c[18]);
    dig_P8 = (int16_t) ((c[21] << 8) | c[20]);
    dig_P9 = (int16_t) ((c[23] << 8) | c[22]);
    /* c[24] is reserved */
    dig_H1 = c[25];

    dig_H2 = (int16_t)((h[1] << 8) | h[0]);
    dig_H3 = h[2];
    dig_H4 = (int16_t)(((int16_t)(int8_t)h[3] * 16) | (int16_t)(h[4] & 0x0Fu));
    dig_H5 = (int16_t)(((int16_t)(int8_t)h[5] * 16) | (int16_t)(h[4] >> 4));
    dig_H6 = (int8_t)h[6];
}

uint8_t bme280_init(void)
{
    /* Auto-detect the address */
    if (HAL_I2C_IsDeviceReady(&hi2c1, 0x76u << 1, 3u, 100u) == HAL_OK)
    {
        bme_addr = 0x76u << 1;
    }
    else if (HAL_I2C_IsDeviceReady(&hi2c1, 0x77u << 1, 3u, 100u) == HAL_OK)
    {
        bme_addr = 0x77u << 1;
    }
    else
    {
        bme_addr = 0u;
        return 0u;
    }

    uint8_t chip_id = 0u;
    if (bme_read_regs(BME280_REG_ID, &chip_id, 1u) != HAL_OK || chip_id != 0x60u)
    {
        bme_addr = 0u;
        return 0u;
    }

    bme_read_calibration();

    bme_write_reg(BME280_REG_CONFIG,   0xA0u);   /* standby/filter — applies in sleep mode */
    bme_write_reg(BME280_REG_CTRL_HUM, 0x01u);   /* humidity x1 */
    /* Do NOT enable normal mode — we measure forced in bme280_read */

    return 1u;
}

/* --- Compensation (integer math, from the BME280 datasheet) --- */

static int32_t compensate_temp(int32_t adc_T)
{
    int32_t var1, var2, T;

    var1 = ((((adc_T >> 3) - ((int32_t)dig_T1 << 1))) * ((int32_t)dig_T2)) >> 11;
    var2 = (((((adc_T >> 4) - ((int32_t)dig_T1)) *
              ((adc_T >> 4) - ((int32_t)dig_T1))) >> 12) * ((int32_t)dig_T3)) >> 14;

    t_fine = var1 + var2;
    T = (t_fine * 5 + 128) >> 8;
    return T;                          /* hundredths of °C */
}

static uint32_t compensate_hum(int32_t adc_H)
{
    int32_t v = t_fine - (int32_t)76800;

    v = (((((adc_H << 14) - (((int32_t)dig_H4) << 20) - (((int32_t)dig_H5) * v)) +
           ((int32_t)16384)) >> 15) *
         (((((((v * ((int32_t)dig_H6)) >> 10) *
             (((v * ((int32_t)dig_H3)) >> 11) + ((int32_t)32768))) >> 10) +
            ((int32_t)2097152)) * ((int32_t)dig_H2) + 8192) >> 14));

    v = v - (((((v >> 15) * (v >> 15)) >> 7) * ((int32_t)dig_H1)) >> 4);
    v = (v < 0) ? 0 : v;
    v = (v > 419430400) ? 419430400 : v;

    return (uint32_t)(v >> 12);        /* %RH x 1024 */
}

static uint32_t compensate_press(int32_t adc_P)
{
    int32_t var1, var2;
    uint32_t p;

    var1 = (((int32_t)t_fine) >> 1) - (int32_t)64000;
    var2 = (((var1 >> 2) * (var1 >> 2)) >> 11) * ((int32_t)dig_P6);
    var2 = var2 + ((var1 * ((int32_t)dig_P5)) << 1);
    var2 = (var2 >> 2) + (((int32_t)dig_P4) << 16);
    var1 = (((dig_P3 * (((var1 >> 2) * (var1 >> 2)) >> 13)) >> 3) +
            ((((int32_t)dig_P2) * var1) >> 1)) >> 18;
    var1 = ((((32768 + var1)) * ((int32_t)dig_P1)) >> 15);

    if (var1 == 0)
    {
        return 0;                      /* avoid division by zero */
    }

    p = (((uint32_t)(((int32_t)1048576) - adc_P) - (var2 >> 12))) * 3125u;
    if (p < 0x80000000u)
    {
        p = (p << 1) / ((uint32_t)var1);
    }
    else
    {
        p = (p / (uint32_t)var1) * 2u;
    }

    var1 = (((int32_t)dig_P9) * ((int32_t)(((p >> 3) * (p >> 3)) >> 13))) >> 12;
    var2 = (((int32_t)(p >> 2)) * ((int32_t)dig_P8)) >> 13;
    p = (uint32_t)((int32_t)p + ((var1 + var2 + dig_P7) >> 4));

    return p;                          /* pascals */
}

HAL_StatusTypeDef bme280_read(bme280_data_t *out)
{
    uint8_t d[8];
    HAL_StatusTypeDef st;

    if (bme_addr == 0u)
    {
        return HAL_ERROR;
    }

    /* Forced mode: rewrite ctrl_hum + ctrl_meas, trigger a single measurement */
    bme_write_reg(BME280_REG_CTRL_HUM,  0x01u);
    bme_write_reg(BME280_REG_CTRL_MEAS, 0x25u);   /* temp x1, pressure x1, FORCED mode */
    HAL_Delay(10u);                                /* time for the measurement (~8 ms) */

    st = bme_read_regs(BME280_REG_DATA, d, 8u);
    if (st != HAL_OK)
    {
        return st;
    }

    /* Raw 20-bit values: pressure d[0..2], temp d[3..5], humidity 16-bit d[6..7] */
    int32_t adc_P = ((int32_t)d[0] << 12) | ((int32_t)d[1] << 4) | (d[2] >> 4);
    int32_t adc_T = ((int32_t)d[3] << 12) | ((int32_t)d[4] << 4) | (d[5] >> 4);
    int32_t adc_H = ((int32_t)d[6] << 8)  | d[7];

    /* Order matters: temperature sets t_fine for the rest */
    out->temp_c100   = (int16_t)compensate_temp(adc_T);
    out->pressure_pa = compensate_press(adc_P);
    out->humidity    = (uint8_t)((compensate_hum(adc_H) + 512u) / 1024u);  /* round to % */

    return HAL_OK;
}