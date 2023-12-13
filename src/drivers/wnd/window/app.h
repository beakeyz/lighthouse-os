#ifndef __ANIVA_LWND_APP_WINDOW__
#define __ANIVA_LWND_APP_WINDOW__

#include "LibGfx/include/driver.h"
#include "drivers/wnd/screen.h"
#include "drivers/wnd/window.h"
#include "proc/proc.h"

void create_test_app(lwnd_screen_t* screen);

window_id_t create_app_lwnd_window(lwnd_screen_t* screen, uint32_t startx, uint32_t starty, lwindow_t* uwindow, proc_t* process);

#endif // !__ANIVA_LWND_APP_WINDOW__
