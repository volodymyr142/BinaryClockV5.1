#ifndef CLOCKFACE_H
#define CLOCKFACE_H

#include "main.h"

/* Position in the 74HC595 chain (1..24) -> frame bit = position - 1 */
#define FACE_BIT_ALARM      0u
#define FACE_SHIFT_H_TENS   1u
#define FACE_SHIFT_H_UNITS  3u
#define FACE_SHIFT_M_TENS   7u
#define FACE_SHIFT_M_UNITS  11u
#define FACE_SHIFT_MODE     15u
#define FACE_SHIFT_BARO     19u

uint32_t face_build(uint8_t hours, uint8_t minutes, uint8_t mode, uint8_t alarm_on, uint8_t baro);

#endif /* CLOCKFACE_H */