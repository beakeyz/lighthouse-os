#ifndef __ANIVA_KTERM_DRIVER__
#define __ANIVA_KTERM_DRIVER__

#include "dev/driver.h"

#define KTERM_DRV_DRAW_STRING   10
#define KTERM_DRV_MAP_FB        11
#define KTERM_DRV_CLEAR         12

extern aniva_driver_t g_base_kterm_driver;

void println_kterm(const char*);

#endif // !__ANIVA_KTERM_DRIVER__
