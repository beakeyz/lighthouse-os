#include "dev/video/framebuffer.h"
#include "drivers/env/lwnd/drawing/draw.h"
#include "window.h"

/*
 * Code that handles breaking up windows into smaller rectangles
 *
 * NOTE: Window coords are relative to their screen, but the window rect coords are relative to the
 * window position which they make up
 */

static inline bool __is_point_in_rect(u32 rx, u32 ry, u32 rw, u32 rh, u32 px, u32 py)
{
    return (px >= rx && px <= (rx + rw)) && (py >= ry && py <= (ry + rh));
}

static inline u32 __abs(i32 i)
{
    return (i < 0) ? -i : i;
}

static inline u32 __max(u32 a, u32 b)
{
    return (a > b) ? a : b;
}

static inline u32 __min(u32 a, u32 b)
{
    return (a < b) ? a : b;
}

static int lwnd_get_overlap_rect(lwnd_window_t* w0, lwnd_window_t* w1, lwnd_wndrect_t* p_rect)
{
    u32 last_y = __max(w0->y, w1->y);
    u32 last_x = __max(w0->x, w1->x);
    u32 first_endx = __min(w1->x + w1->width, w0->x + w0->width);
    u32 first_endy = __min(w1->y + w1->height, w0->y + w0->height);

    if ((i32)(first_endx - last_x) <= 0)
        return -1;

    if ((i32)(first_endy - last_y) <= 0)
        return -1;

    p_rect->x = __abs(last_x - w0->x);
    p_rect->y = __abs(last_y - w0->y);

    p_rect->w = first_endx - last_x;
    p_rect->h = first_endy - last_y;
    // TODO: ???
    return 0;
}

int lwnd_window_split(fb_info_t* info, lwnd_window_t* wnd)
{
    /* Loop over all previous layers */
    for (lwnd_window_t* p = wnd->prev_layer; p != nullptr; p = p->prev_layer) {
        lwnd_wndrect_t overlap;

        /* Check if the fuckers overlap */
        if (lwnd_get_overlap_rect(wnd, p, &overlap))
            continue;

        lwnd_draw_dbg_box(info, wnd->x + overlap.x, wnd->y + overlap.y, overlap.w, overlap.h, (fb_color_t) { { 0, 0, 0xff, 0xff } });
    }
    return 0;
}
