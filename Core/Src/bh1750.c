#include "bh1750.h"

extern I2C_HandleTypeDef hi2c1;

#define BH1750_TIMEOUT_MS     100u
#define BH1750_CMD_POWER_ON   0x01u
#define BH1750_CMD_CONT_HRES  0x10u   /* continuous, high resolution (1 lux) */

/* 8-bit address (already shifted). 0 = not found */
static uint8_t bh_addr = 0u;

static HAL_StatusTypeDef bh_cmd(uint8_t cmd)
{
    return HAL_I2C_Master_Transmit(&hi2c1, bh_addr, &cmd, 1u, BH1750_TIMEOUT_MS);
}

uint8_t bh1750_init(void)
{
    /* Auto-detect the address: try 0x23 first, then 0x5C */
    if (HAL_I2C_IsDeviceReady(&hi2c1, 0x23u << 1, 3u, 100u) == HAL_OK)
    {
        bh_addr = 0x23u << 1;
    }
    else if (HAL_I2C_IsDeviceReady(&hi2c1, 0x5Cu << 1, 3u, 100u) == HAL_OK)
    {
        bh_addr = 0x5Cu << 1;
    }
    else
    {
        bh_addr = 0u;
        return 0u;
    }

    bh_cmd(BH1750_CMD_POWER_ON);
    bh_cmd(BH1750_CMD_CONT_HRES);
    return 1u;
}

HAL_StatusTypeDef bh1750_read_raw(uint16_t *raw)
{
    uint8_t buf[2];
    HAL_StatusTypeDef st;

    if (bh_addr == 0u)
    {
        return HAL_ERROR;
    }

    st = HAL_I2C_Master_Receive(&hi2c1, bh_addr, buf, 2u, BH1750_TIMEOUT_MS);
    if (st != HAL_OK)
    {
        return st;
    }

    *raw = (uint16_t)(((uint16_t)buf[0] << 8) | buf[1]);
    return HAL_OK;
}

uint16_t bh1750_lux(uint16_t raw)
{
    /* Per the datasheet: lux = raw / 1.2.
       Dividing by a fraction is expensive without an FPU, so /1.2 = *5/6 */
    return (uint16_t)(((uint32_t)raw * 5u) / 6u);
}