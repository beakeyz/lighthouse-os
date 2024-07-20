#ifndef __ANIVA_LWND_APP_WINDOW__
#define __ANIVA_LWND_APP_WINDOW__

#include "libgfx/shared.h"
#include "drivers/env/lwnd/screen.h"
#include "drivers/env/lwnd/window.h"
#include "proc/proc.h"

window_id_t create_app_lwnd_window(lwnd_screen_t* screen, uint32_t startx, uint32_t starty, lwindow_t* uwindow, proc_t* process);

#endif // !__ANIVA_LWND_APP_WINDOW__
