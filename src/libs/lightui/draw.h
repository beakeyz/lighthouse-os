#ifndef __LIGHTOS_LIGHTUI_DRAWING__
#define __LIGHTOS_LIGHTUI_DRAWING__

#include "libgfx/video.h"
#include "lightui/window.h"
#include <stdint.h>

LEXPORT int lightui_draw_rect(lightui_window_t* wnd, uint32_t x, uint32_t y, uint32_t width, uint32_t height, lcolor_t c);
LEXPORT int lightui_draw_buffer(lightui_window_t* wnd, uint32_t x, uint32_t y, lclr_buffer_t* buffer);

#endif // !__LIGHTOS_LIGHTUI_DRAWING__
