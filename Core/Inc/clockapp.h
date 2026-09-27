#ifndef CLOCKAPP_H
#define CLOCKAPP_H

#include "main.h"

/* Init: read time from the DS3231, prepare the state machine */
void clockapp_init(void);

/* Main application tick. Call as often as possible from while(1).
   Handles: button polling, state machine, display update. */
void clockapp_task(void);

#endif /* CLOCKAPP_H */