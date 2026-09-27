#include "buzzer.h"

static uint8_t  active;
static uint8_t  infinite;
static uint16_t beeps_left;
static uint16_t t_on, t_off;
static uint8_t  phase;         /* 1 = tone, 2 = pause */
static uint32_t phase_until;

static void pin(uint8_t on)
{
    HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void buzzer_init(void)
{
    active = 0u;
    phase  = 0u;
    pin(0);
}

void buzzer_beep(uint16_t count, uint16_t on_ms, uint16_t off_ms)
{
    t_on       = on_ms;
    t_off      = off_ms;
    infinite   = (count == 0u) ? 1u : 0u;
    beeps_left = count;
    active     = 1u;
    phase      = 1u;
    phase_until = HAL_GetTick() + t_on;
    pin(1);
}

void buzzer_stop(void)
{
    active = 0u;
    phase  = 0u;
    pin(0);
}

uint8_t buzzer_is_active(void)
{
    return active;
}

void buzzer_task(void)
{
    if (!active)
    {
        return;
    }
    if ((int32_t)(HAL_GetTick() - phase_until) < 0)
    {
        return;                       /* current phase is still running */
    }

    if (phase == 1u)                  /* tone just finished */
    {
        pin(0);
        if (!infinite)
        {
            if (beeps_left > 0u) { beeps_left--; }
            if (beeps_left == 0u) { buzzer_stop(); return; }
        }
        phase = 2u;
        phase_until = HAL_GetTick() + t_off;
    }
    else                              /* pause just finished */
    {
        pin(1);
        phase = 1u;
        phase_until = HAL_GetTick() + t_on;
    }
}