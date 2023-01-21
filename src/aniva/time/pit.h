#ifndef __ANIVA_PIT__
#define __ANIVA_PIT__
#include "libk/error.h"
#include <libk/stddef.h>

#define PIT_TIMER_INT_NUM 0

ANIVA_STATUS set_pit_interrupt_handler();
void set_pit_frequency(size_t, bool);
bool pit_frequency_capability(size_t);
void set_pit_periodic(bool);

void init_and_install_pit();

#endif // !__ANIVA_PIT__