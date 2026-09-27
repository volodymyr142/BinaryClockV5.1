#ifndef BRIGHTNESS_H
#define BRIGHTNESS_H

#include "main.h"

void brightness_init(void);

/* Call periodically from the main loop.
   Decides on its own when it's time to read the sensor and adjust brightness. */
void brightness_task(void);

#endif /* BRIGHTNESS_H */