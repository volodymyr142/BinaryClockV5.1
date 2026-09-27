#include "buttons.h"

extern ADC_HandleTypeDef hadc;

#define BTN_DEBOUNCE_MS   20U

static button_id_t adc_to_button(uint16_t v)
{
    if (v >= 3820u) { return BTN_COUNT; }   /* nothing */
    if (v >= 3150u) { return BTN_MODE; }    /* b1 ≈ 3560 */
    if (v >= 2200u) { return BTN_PREV; }    /* b2 ≈ 2744 */
    if (v >= 1175u) { return BTN_NEXT; }    /* b3 ≈ 1665 */
    return BTN_SET;                         /* b4 ≈ 686  */
}

static uint16_t adc_read(void)
{
    uint16_t v = 0u;
    HAL_ADC_Start(&hadc);
    if (HAL_ADC_PollForConversion(&hadc, 5u) == HAL_OK)
    {
        v = (uint16_t)HAL_ADC_GetValue(&hadc);
    }
    HAL_ADC_Stop(&hadc);
    return v;
}

static uint8_t     initialized = 0u;
static button_id_t raw_btn    = BTN_COUNT;
static uint32_t    raw_since  = 0u;
static button_id_t stable_btn = BTN_COUNT;
static uint32_t    down_since[BTN_COUNT];
static uint8_t     edge_down[BTN_COUNT];
static uint8_t     edge_up[BTN_COUNT];

void buttons_update(void)
{
    uint32_t now = HAL_GetTick();

    if (!initialized)
    {
        HAL_ADCEx_Calibration_Start(&hadc);
        initialized = 1u;
    }

    button_id_t r = adc_to_button(adc_read());
    if (r != raw_btn)   
    {
        raw_btn   = r;
        raw_since = now;
        return;
    }

    if ((uint32_t)(now - raw_since) < BTN_DEBOUNCE_MS)
    {
        return;
    }

    if (r != stable_btn) 
    {
        if (stable_btn != BTN_COUNT) { edge_up[stable_btn] = 1u; }
         stable_btn = r;
         if (r != BTN_COUNT)
         {
            down_since[r] = now;
            edge_down[r]  = 1u;
         }
    }
}

uint8_t button_pressed(button_id_t id)
{
    uint8_t e = edge_down[id];
    edge_down[id] = 0u;
    return e;
}

uint8_t button_released(button_id_t id)
{
    uint8_t e = edge_up[id];
    edge_up[id] = 0u;
    return e;
}

uint8_t button_is_down(button_id_t id)
{
     return (stable_btn == id) ? 1u : 0u;
}

uint32_t button_down_ms(button_id_t id)
{
    if (stable_btn != id)
    {
        return 0u;
    }

    return (uint32_t)(HAL_GetTick() - down_since[id]);
}