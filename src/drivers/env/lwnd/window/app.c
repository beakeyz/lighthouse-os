#include "app.h"
#include "libgfx/shared.h"
#include "dev/video/framebuffer.h"
#include "drivers/env/lwnd/window.h"
#include "sync/mutex.h"

#define APP_BAR_HEIGHT 16

static fb_color_t _app_bar_color = (fb_color_t) { .raw_clr = 0xFF1f1f1f };
static fb_color_t _app_bar_outline_clr = (fb_color_t) { .raw_clr = 0xff454545 };

/*!
 * @brief: Responsible for drawing the apps framebuffer together with its top bar
 */
int lwnd_app_draw(lwnd_window_t* window)
{
    /* Make sure the app bar is filled in */
    for (uint32_t i = 0; i < APP_BAR_HEIGHT; i++) {
        for (uint32_t j = 0; j < window->width; j++) {
            if (!i || !j || i == APP_BAR_HEIGHT - 1 || j == window->width - 1)
                *((fb_color_t*)window->fb_ptr + (i * window->width) + j) = _app_bar_outline_clr;
            else
                *((fb_color_t*)window->fb_ptr + (i * window->width) + j) = _app_bar_color;
        }
    }

    /* TODO: draw close button */
    /* TODO: draw hide button */
    /* TODO: draw fullscreen button */

    return 0;
}

lwnd_window_ops_t lwnd_app_ops = {
    .f_draw = lwnd_app_draw,
};

/*!
 * @brief: Creates and registers a window for a process
 *
 * @returns: the window_id of the created window
 */
window_id_t create_app_lwnd_window(lwnd_screen_t* screen, uint32_t startx, uint32_t starty, lwindow_t* uwindow, proc_t* process)
{
    lwnd_window_t* wnd;

    if (!uwindow || !process)
        return LWND_INVALID_ID;

    uwindow->current_height += APP_BAR_HEIGHT;

    if (uwindow->wnd_flags & LWND_FLAG_CENTERED) {
        startx = (screen->width >> 1) - (uwindow->current_width >> 1);
        starty = (screen->height >> 1) - (uwindow->current_height >> 1);
    }

    /* Create a generic window with every button on its bar */
    wnd = create_lwnd_window(screen, startx, starty, uwindow->current_width, uwindow->current_height, LWND_WNDW_HIDE_BTN | LWND_WNDW_FULLSCREEN_BTN | LWND_WNDW_CLOSE_BTN, LWND_TYPE_PROCESS, process);

    if (!wnd)
        return LWND_INVALID_ID;

    mutex_lock(screen->draw_lock);

    wnd->label = process->m_name;

    lwnd_window_set_ops(wnd, &lwnd_app_ops);
    lwnd_request_framebuffer(wnd);

    /* Set the pointer to the user framebuffer to under the appbar */
    wnd->user_fb_ptr = wnd->user_real_fb_ptr + (APP_BAR_HEIGHT * wnd->width * sizeof(uint32_t));
    uwindow->current_height -= APP_BAR_HEIGHT;

    lwnd_window_move(wnd, startx, starty);

    mutex_unlock(screen->draw_lock);
    return wnd->uid;
}
