#ifndef __LIGHTENV_GFX_VIDEO__
#define __LIGHTENV_GFX_VIDEO__

#include "libgfx/shared.h"
#include "sys/types.h"

/*
 * Format for a single pixel
 */
typedef union lcolor {
    struct {
        uint8_t b, g, r, a;
    };
    uint32_t raw;
} lcolor_t;

#define LCLR_RGBA(_r, _g, _b, _a) ((lcolor_t) { .r = (_r), .g = (_g), .b = (_b), .a = (_a) })

BOOL lwindow_request_framebuffer(lwindow_t* wnd, lframebuffer_t* fb);

BOOL lwindow_update(lwindow_t* wnd);
BOOL lwindow_force_update(lwindow_t* wnd);

static inline BOOL lwindow_has_fb(lwindow_t* wnd)
{
    return ((wnd->wnd_flags & LWND_FLAG_HAS_FB) == LWND_FLAG_HAS_FB);
}

BOOL lwindow_resize(lwindow_t* wnd, uint32_t new_width, uint32_t new_height);
BOOL lwindow_draw_rect(lwindow_t* wnd, uint32_t x, uint32_t y, uint32_t width, uint32_t height, lcolor_t clr);
BOOL lwindow_draw_outline_rect(lwindow_t* wnd, uint32_t x, uint32_t y, uint32_t width, uint32_t height, lcolor_t clr);
BOOL lwindow_draw_line(lwindow_t* wnd, uint32_t startx, uint32_t starty, uint32_t endx, uint32_t endy, uint32_t thickness, lcolor_t clr);
BOOL lwindow_draw_image(lwindow_t* wnd, uint32_t startx, uint32_t starty, HANDLE image_handle);
BOOL lwindow_draw_buffer(lwindow_t* wnd, uint32_t startx, uint32_t starty, lclr_buffer_t* buffer);

#endif // !__LIGHTENV_GFX_VIDEO__
