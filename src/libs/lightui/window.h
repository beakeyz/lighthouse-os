#ifndef __LIGHTOS_LIGHTUI_WINDOW__
#define __LIGHTOS_LIGHTUI_WINDOW__

#include "libgfx/shared.h"
#include "lightos/lib/lightos.h"
#include <stdint.h>

struct lightui_widget;

#define LIGHTUI_WNDFLAG_MAXIMIZED 0x00000001
#define LIGHTUI_WNDFLAG_MINIMIZED 0x00000002
#define LIGHTUI_WNDFLAG_NO_MIN_BTN 0x00000004
#define LIGHTUI_WNDFLAG_NO_MAX_BTN 0x00000008
#define LIGHTUI_WNDFLAG_NO_CLOSE_BTN 0x00000010
#define LIGHTUI_WNDFLAG_NO_BORDER 0x00000020
#define LIGHTUI_WNDFLAG_NO_TOPBAR 0x00000040
#define LIGHTUI_WNDFLAG_DEFER_FB 0x00000080
/* When set, the window is in widget mode */
#define LIGHTUI_WNDFLAG_WIDGET 0x00000100
/* Marks that there are widgets that need to update */
#define LIGHTUI_WNDFLAG_WIDGETS_NEED_UPDATE 0x00000200

#define LIGHTUI_TOP_BORDER_HEIGHT 18
#define LIGHTUI_BORDER_WIDTH 4

typedef struct lightui_window {
    /* Window struct used by libgfx */
    lwindow_t gfxwnd;
    /* Our window flags */
    uint32_t lui_flags;
    uint32_t lui_top_border_height;
    uint32_t lui_border_width;
    uint32_t lui_height;
    uint32_t lui_width;
    /*
     * Pointer inside the framebuffer for this window, where
     * the actual buffer starts for the process
     */
    void* user_fb_start;

    /* List of widgets attached to this window */
    struct lightui_widget* widgets;
} lightui_window_t;

static inline void lightui_window_request_widget_update(lightui_window_t* wnd)
{
    wnd->lui_flags |= LIGHTUI_WNDFLAG_WIDGETS_NEED_UPDATE;
}

/*!
 * @brief: Request a window to be created on the display manager
 *
 */
LEXPORT lightui_window_t* lightui_request_window(const char* label, uint32_t width, uint32_t height, uint32_t lui_flags);
LEXPORT int lightui_close_window(lightui_window_t* wnd);

LEXPORT int lightui_window_update(lightui_window_t* wnd);

LEXPORT int lightui_request_fb(lightui_window_t* wnd);
LEXPORT int lightui_move_window(lightui_window_t* wnd, uint32_t x, uint32_t y);
LEXPORT int lightui_maximize_window(lightui_window_t* wnd);
LEXPORT int lightui_minimize_window(lightui_window_t* wnd);
LEXPORT int lightui_normalize_window(lightui_window_t* wnd);

#endif // !__LIGHTOS_LIGHTUI_WINDOW__
