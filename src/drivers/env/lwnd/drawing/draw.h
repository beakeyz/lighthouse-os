#ifndef __ANIVA_DRIVER_LWND_DRAWING__
#define __ANIVA_DRIVER_LWND_DRAWING__

#include "dev/video/framebuffer.h"
#include "drivers/env/lwnd/display/screen.h"
#include "libk/stddef.h"

extern int lwnd_draw_line(lwnd_screen_t* screen, u32 x1, u32 y1, u32 x2, u32 y2, fb_color_t clr);
extern int lwnd_draw_dbg_box(lwnd_screen_t* screen, u32 x, u32 y, u32 w, u32 h, fb_color_t clr);

#endif // !__ANIVA_DRIVER_LWND_DRAWING__
