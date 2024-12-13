#include "libgfx/lgfx.h"
#include "libgfx/shared.h"
#include "libgfx/video.h"
#include "lightui/widget.h"
#include "lightui/widgets/button.h"
#include "lightui/window.h"
#include <errno.h>
#include <lightos/lightos.h>
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

    /* If the borderwidth is NULL, we don't draw any extra shit lol */
    if (!wnd->lui_border_width)
        return 0;

    /* Prepare the top bar */
    lwindow_draw_outline_rect(&wnd->gfxwnd, 0, 0, wnd->gfxwnd.current_width, wnd->gfxwnd.current_height, LCLR_RGBA(0x0f, 0x0f, 0x0f, 0xff));
    lwindow_draw_outline_rect(&wnd->gfxwnd, 1, 1, wnd->gfxwnd.current_width - 2, wnd->gfxwnd.current_height - 2, LCLR_RGBA(0x2f, 0x2f, 0x2f, 0xff));
    lwindow_draw_rect(&wnd->gfxwnd, 2, 2, wnd->gfxwnd.current_width - 4, wnd->gfxwnd.current_height - 3, LCLR_RGBA(0x1f, 0x1f, 0x1f, 0xff));

    if (wnd->lui_top_border_height)
        lwindow_draw_outline_rect(&wnd->gfxwnd, wnd->lui_border_width - 1, wnd->lui_top_border_height - 1, wnd->lui_width + 2, wnd->lui_height + 2, LCLR_RGBA(0x0f, 0x0f, 0x0f, 0xff));

    return 0;
}

lightui_window_t* lightui_request_window(const char* label, uint32_t width, uint32_t height, uint32_t lui_flags)
{
    lightui_window_t* wnd;

    /* TODO: Reallocate */
    if (n_window == window_list_sz)
        return nullptr;

    wnd = __get_ligthui_wnd_slot();

    if (!wnd)
        return nullptr;

    /* Set the general border width */
    wnd->lui_border_width = (((lui_flags & LIGHTUI_WNDFLAG_NO_BORDER) == LIGHTUI_WNDFLAG_NO_BORDER)
            ? 0
            : LIGHTUI_BORDER_WIDTH);

    /* Set the top bar height */
    wnd->lui_top_border_height = (((lui_flags & LIGHTUI_WNDFLAG_NO_TOPBAR) == LIGHTUI_WNDFLAG_NO_TOPBAR)
            ? LIGHTUI_BORDER_WIDTH
            : LIGHTUI_TOP_BORDER_HEIGHT);

    /* Make sure there is no top border, if the borderwidth is zero lmao */
    if (!wnd->lui_border_width)
        wnd->lui_top_border_height = 0;

    /* Add a bit to the height to encompass the border */
    if (!request_lwindow(&wnd->gfxwnd, label, width + (2 * wnd->lui_border_width), height + wnd->lui_top_border_height + wnd->lui_border_width, LWND_FLAG_DEFER_UPDATES))
        return nullptr;

    wnd->lui_flags = lui_flags;
    wnd->lui_width = width;
    wnd->lui_height = height;

    /* Add the close button */
    lightui_add_button_widget(wnd, NULL, (lui_flags & LIGHTUI_WNDFLAG_NO_CLOSE_BTN) ? NULL : "red", 6, 5, 8, 8, NULL);
    /* Add the minimize button */
    lightui_add_button_widget(wnd, NULL, (lui_flags & LIGHTUI_WNDFLAG_NO_MIN_BTN) ? NULL : "orange", 6 + 14, 5, 8, 8, NULL);
    /* Add the maximize button */
    lightui_add_button_widget(wnd, NULL, (lui_flags & LIGHTUI_WNDFLAG_NO_MAX_BTN) ? NULL : "green", 6 + 28, 5, 8, 8, NULL);

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

/*!
 * @brief: Checks if a window needs to update it's widgets
 */
static inline void lwindow_maybe_update_widgets(lightui_window_t* wnd)
{
    /* Widget update flag not set, no need to update */
    if (!(wnd->lui_flags & LIGHTUI_WNDFLAG_WIDGETS_NEED_UPDATE))
        return;

    /* Loop over all widgets to update them */
    for (lightui_widget_t* widget = wnd->widgets; widget; widget = widget->next)
        widget->f_draw(widget);

    /* Clear the update flag to prevent unwanted updates */
    wnd->lui_flags &= ~LIGHTUI_WNDFLAG_WIDGETS_NEED_UPDATE;
}

/*!
 * @brief: Push the window framebuffer to the screen
 *
 * First updates any widgets attached to this window, then
 * forces the pixel updates
 */
int lightui_window_update(lightui_window_t* wnd)
{
    lwindow_maybe_update_widgets(wnd);

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
