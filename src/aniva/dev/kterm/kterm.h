#ifndef __ANIVA_KTERM_DRIVER__
#define __ANIVA_KTERM_DRIVER__
#include "dev/driver.h"

#define KTERM_DRV_DRAW_STRING 10

extern aniva_driver_t g_base_kterm_driver;

void println_kterm(const char*);

#endif // !__ANIVA_KTERM_DRIVER__
