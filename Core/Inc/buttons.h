#ifndef BUTTONS_H
#define BUTTONS_H

#include "main.h"

typedef enum
{
    BTN_MODE = 0,
    BTN_PREV,
    BTN_NEXT,
    BTN_SET,
    BTN_COUNT          /* always last: gives the button count */
} button_id_t;

/* Call as often as possible from the main loop */
void buttons_update(void);

/* Press edge. Read ONCE: the flag clears after being read */
uint8_t button_pressed(button_id_t id);

/* Release edge, also one-shot */
uint8_t button_released(button_id_t id);

/* Current debounced state: 1 = the button is down right now */
uint8_t button_is_down(button_id_t id);

/* How many milliseconds it's been held. 0 if not pressed */
uint32_t button_down_ms(button_id_t id);

#endif /* BUTTONS_H */