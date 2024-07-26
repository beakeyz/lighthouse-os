#include "libgfx/video.h"
#include "lightui/draw.h"
#include <lightui/window.h>
#include <stddef.h>

/*
 * This is a demo app that should utilise the compositor and the
 * graphics API to draw a smily face on a canvas of 75x75
 */

lightui_window_t* wnd;

int main()
{
    wnd = lightui_request_window("gfxtest", 100, 100, NULL);

    if (!wnd)
        return -1;

    lightui_draw_rect(wnd, 0, 0, 640, 400, LCLR_RGBA(0xff, 0xff, 0xff, 0xff));

    /* Draw the smily */

    lightui_draw_rect(wnd, 10, 10, 10, 10, LCLR_RGBA(0xA5, 0xFF, 0, 0xFF));
    lightui_draw_rect(wnd, 50, 10, 10, 10, LCLR_RGBA(0xA5, 0xFF, 0, 0xFF));

    lightui_draw_rect(wnd, 10, 30, 10, 10, LCLR_RGBA(0xA5, 0xFF, 0, 0xFF));
    lightui_draw_rect(wnd, 50, 30, 10, 10, LCLR_RGBA(0xA5, 0xFF, 0, 0xFF));

    lightui_draw_rect(wnd, 15, 35, 40, 10, LCLR_RGBA(0xA5, 0xFF, 0, 0xFF));

    lightui_window_update(wnd);

    while (true) { }

    lightui_close_window(wnd);
    return 0;
}
