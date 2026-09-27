#include "shift595.h"

extern TIM_HandleTypeDef htim14;

/* ---- Small pin helpers ---- */

static inline void sr595_pin_data(uint8_t level)
{
    HAL_GPIO_WritePin(LED_DATA_GPIO_Port, LED_DATA_Pin,
                      level ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static inline void sr595_pin_clock(uint8_t level)
{
    HAL_GPIO_WritePin(LED_CLOCK_GPIO_Port, LED_CLOCK_Pin,
                      level ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static inline void sr595_pin_latch(uint8_t level)
{
    HAL_GPIO_WritePin(LED_LATCH_GPIO_Port, LED_LATCH_Pin,
                      level ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* ---- Public API ---- */

void sr595_set_brightness(uint8_t level)
{
    /* PWM mode 1: output is high while CNT < CCR.
       /OE is active-low -> low = lit.
       level 255 -> CCR 0   -> always low  -> full brightness;
       level 0   -> CCR 255 -> almost always high -> almost off. */
    __HAL_TIM_SET_COMPARE(&htim14, TIM_CHANNEL_1, (uint32_t)(255u - level));
}

void sr595_write(uint32_t frame)
{
    /* Shift out from the most significant frame bit to the least significant. */
    for (uint8_t i = SR595_BIT_COUNT; i > 0u; i--)
    {
        uint8_t bit = (uint8_t)((frame >> (i - 1u)) & 1u);

        sr595_pin_data(bit);    /* 1. set the bit value */
        sr595_pin_clock(1);     /* 2. rising edge: shift register latches the bit */
        sr595_pin_clock(0);     /* 3. clock back to 0 for the next bit */
    }

    /* Whole frame is in the shift register now — latch it to the outputs. */
    sr595_pin_latch(1);
    sr595_pin_latch(0);
}

void sr595_init(void)
{
    HAL_TIM_PWM_Start(&htim14, TIM_CHANNEL_1);   /* start PWM generation on /OE */

    sr595_set_brightness(0u);      /* nearly off while filling the chain */

    sr595_pin_clock(0);
    sr595_pin_latch(0);
    sr595_pin_data(0);

    sr595_write(0u);          /* fill the chain with zeros and latch */

    sr595_set_brightness(200u);    /* startup brightness */
}