#include "libgfx/video.h"
#include "lightos/dynamic.h"
#include "lightui/draw.h"
#include <lightui/window.h>
#include <stdio.h>

/*
 * This is a demo app that should utilise the compositor and the
 * graphics API to draw a smily face on a canvas of 75x75
 */

lightui_window_t* wnd;

int main()
{
    wnd = lightui_request_window("gfxtest", 100, 100, LIGHTUI_WNDFLAG_NO_MIN_BTN);

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

    HANDLE kterm_handle = HNDL_INVAL;
    int (*p_kterm_update_box)(uint32_t p_id, uint32_t x, uint32_t y, uint8_t color_idx, const char* title, const char* content);

    printf("Trying to load kterm.lib\n");

    if (!load_library("kterm.lib", &kterm_handle))
        goto close_and_quit;

    printf("Trying to get address\n");

    /* Get the function address */
    if (!get_func_address(kterm_handle, "kterm_update_box", (void**)&p_kterm_update_box))
        goto close_and_quit;

    p_kterm_update_box(0, 4, 4, 0, "Hello", "World!");

    while (true) { }

close_and_quit:
    lightui_close_window(wnd);
    return 0;
}
