#include "libgfx/lgfx.h"
#include "libgfx/shared.h"
#include "libgfx/video.h"
#include "lightui/window.h"
#include <errno.h>
#include <lightos/lib/lightos.h>
#include <stdlib.h>
#include <string.h>

static uint32_t window_list_sz;
static uint32_t n_window;
static uint32_t highest_window_id;
static lightui_window_t* window_list;
/*!
 * @brief: Entry function for the lightUI library
 *
 * This library is an extention uppon the pretty low-level libgfx library, which handles
 * communication with the window management driver. This driver aims to standardise UI
 * and graphics development, window layout, user sided window management, ect.
 *
 * When apps create windows through this library, they will gain:
 *  - An indicator for when their window is focussed
 *  - A functional window overlay (close, minimize, maximize buttons, resize capabilities, ect.)
 *  - A widget library to create standardized interfaces
 *
 */
LIGHTENTRY int lib_entry(void)
{
    window_list_sz = 32;
    n_window = 0;
    highest_window_id = 0;
    window_list = malloc(sizeof(*window_list) * window_list_sz);

    if (!window_list)
        return -ENOMEM;

    memset(window_list, 0, sizeof(*window_list) * window_list_sz);

    return 0;
}

static lightui_window_t* __get_ligthui_wnd_slot()
{
    /* Desync, just do a scan */
    if (highest_window_id != n_window) {
        for (uint32_t i = 0; i < highest_window_id; i++) {
            if (!window_list[i].gfxwnd.title) {
                return &window_list[i];
            }
        }
    }
    return &window_list[highest_window_id];
}

static int __maybe_request_fb(lightui_window_t* wnd)
{
    /* Already have a fb */
    if ((wnd->lui_flags & LIGHTUI_WNDFLAG_DEFER_FB) == LIGHTUI_WNDFLAG_DEFER_FB)
        return 0;

    if (!lwindow_request_framebuffer(&wnd->gfxwnd, NULL))
        return -EBADE;

    /* Set the pointer for the user */
    wnd->user_fb_start = (void*)wnd->gfxwnd.fb.fb + ((wnd->lui_top_border_height) * wnd->gfxwnd.fb.pitch) + (wnd->lui_border_width * (wnd->gfxwnd.fb.bpp >> 3));

    /* Prepare the top bar */
    lwindow_draw_outline_rect(&wnd->gfxwnd, 0, 0, wnd->gfxwnd.current_width, wnd->gfxwnd.current_height, LCLR_RGBA(0x0f, 0x0f, 0x0f, 0xff));
    lwindow_draw_outline_rect(&wnd->gfxwnd, 1, 1, wnd->gfxwnd.current_width - 2, wnd->gfxwnd.current_height - 2, LCLR_RGBA(0x2f, 0x2f, 0x2f, 0xff));
    lwindow_draw_rect(&wnd->gfxwnd, 2, 2, wnd->gfxwnd.current_width - 4, wnd->gfxwnd.current_height - 3, LCLR_RGBA(0x1f, 0x1f, 0x1f, 0xff));
    lwindow_draw_outline_rect(&wnd->gfxwnd, wnd->lui_border_width - 1, wnd->lui_top_border_height - 1, wnd->lui_width + 2, wnd->lui_height + 2, LCLR_RGBA(0x0f, 0x0f, 0x0f, 0xff));

    return 0;
}

lightui_window_t* lightui_request_window(const char* label, uint32_t width, uint32_t height, uint32_t lui_flags)
{
    lightui_window_t* wnd;
    const uint32_t border_height = (((lui_flags & LIGHTUI_WNDFLAG_NO_BORDER) == LIGHTUI_WNDFLAG_NO_BORDER)
            ? 0
            : LIGHTUI_TOP_BORDER_HEIGHT);

    /* TODO: Reallocate */
    if (n_window == window_list_sz)
        return nullptr;

    wnd = __get_ligthui_wnd_slot();

    if (!wnd)
        return nullptr;

    wnd->lui_border_width = 4;

    /* Add a bit to the height to encompass the border */
    if (!request_lwindow(&wnd->gfxwnd, label, width + (2 * wnd->lui_border_width), height + border_height + wnd->lui_border_width, LWND_FLAG_DEFER_UPDATES))
        return nullptr;

    wnd->lui_top_border_height = border_height;
    wnd->lui_flags = lui_flags;
    wnd->lui_width = width;
    wnd->lui_height = height;

    /* Request a framebuffer if the caller doesn't want to do that themselves */
    if (__maybe_request_fb(wnd)) {
        close_lwindow(&wnd->gfxwnd);
        return nullptr;
    }

    n_window++;
    highest_window_id++;

    return wnd;
}

int lightui_close_window(lightui_window_t* wnd)
{
    if (!wnd)
        return -1;

    /* Remove the window from the display manager */
    close_lwindow(&wnd->gfxwnd);

    /* Clear the slot */
    memset(wnd, 0, sizeof(*wnd));

    /* Release one window */
    n_window--;
    return 0;
}

LEXPORT int lightui_window_update(lightui_window_t* wnd)
{
    if (lwindow_force_update(&wnd->gfxwnd))
        return -EBADMSG;

    return 0;
}

int lightui_request_fb(lightui_window_t* wnd)
{
    wnd->lui_flags &= ~LIGHTUI_WNDFLAG_DEFER_FB;

    if (__maybe_request_fb(wnd))
        return -EBADE;

    return 0;
}

int lightui_move_window(lightui_window_t* wnd, uint32_t x, uint32_t y);
int lightui_maximize_window(lightui_window_t* wnd);
int lightui_minimize_window(lightui_window_t* wnd);
int lightui_normalize_window(lightui_window_t* wnd);
