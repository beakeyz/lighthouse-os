#ifndef __ANIVA_LWND_WINDOWING_PRIV__
#define __ANIVA_LWND_WINDOWING_PRIV__

#include "drivers/env/lwnd/windowing/window.h"

extern void __window_clear_rects(lwnd_window_t* wnd, lwnd_wndrect_t** pstart);
extern void window_clear_rects(lwnd_window_t* wnd);

#endif // !__ANIVA_LWND_WINDOWING_PRIV__
