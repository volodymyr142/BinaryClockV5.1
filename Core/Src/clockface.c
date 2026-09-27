#include "clockface.h"

static uint32_t field(uint8_t value, uint8_t width, uint8_t shift)
{
    uint32_t bits = 0u;

    for (uint8_t i = 0u; i < width; i++)
    {
        if (value & (1u << i))
        {
            /* Bit-mirrors the digit within its field to match the matrix wiring order. */
            bits |= 1u << (shift + (width - 1u - i));
        }
    }
    return bits;
}

uint32_t face_build(uint8_t hours, uint8_t minutes, uint8_t mode,
                    uint8_t alarm_on, uint8_t baro)
{
    uint32_t frame = 0u;

    if (alarm_on) { frame |= 1u << FACE_BIT_ALARM; }

    frame |= field(hours   / 10u, 2u, FACE_SHIFT_H_TENS);
    frame |= field(hours   % 10u, 4u, FACE_SHIFT_H_UNITS);
    frame |= field(minutes / 10u, 4u, FACE_SHIFT_M_TENS);

    /* Minute-units LEDs 0 and 1 are swapped on the matrix, so swap those two bits here. */
    uint8_t mu = minutes % 10u;
    uint8_t mu_fix = (uint8_t)((mu & 0x0Cu) | ((mu & 0x01u) << 1) | ((mu & 0x02u) >> 1));
    frame |= field(mu_fix, 4u, FACE_SHIFT_M_UNITS);

    frame |= ((uint32_t)(mode & 0x0Fu)) << FACE_SHIFT_MODE;
    frame |= ((uint32_t)(baro & 0x1Fu)) << FACE_SHIFT_BARO;

    return frame;
}