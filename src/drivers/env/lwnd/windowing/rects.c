#include "dev/video/framebuffer.h"
#include "drivers/env/lwnd/drawing/draw.h"
#include "libk/stddef.h"
#include "mem/zalloc/zalloc.h"
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

static inline int lwnd_get_overlap_rect(lwnd_window_t* w0, lwnd_window_t* w1, lwnd_wndrect_t* p_rect)
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

static inline lwnd_wndrect_t* create_and_link_lwndrect(lwnd_window_t* wnd, u32 x, u32 y, u32 w, u32 h)
{
    lwnd_wndrect_t* rect;

    rect = zalloc_fixed(wnd->rect_cache);

    if (!rect)
        return nullptr;

    rect->next_part = wnd->rects;
    rect->x = x;
    rect->y = y;
    rect->w = w;
    rect->h = h;

    /* Link into the window */
    wnd->rects = rect;

    return rect;
}

static inline void __window_clear_rects(lwnd_window_t* wnd)
{
    lwnd_wndrect_t* next;

    if (!wnd->rects)
        return;

    do {
        next = wnd->rects->next_part;

        /* Free from our cache */
        zfree_fixed(wnd->rect_cache, wnd->rects);

        wnd->rects = next;
    } while (wnd->rects);
}

static inline int __window_split(fb_info_t* info, lwnd_window_t* wnd, lwnd_wndrect_t* rects)
{
    /* Start at the windows topleft */
    u32 c_x = 0;
    u32 c_y = 0;
    u32 c_w = rects->x;
    u32 c_h = rects->y + rects->h;

    /* There might be a max. of 4 rectangles we need to create as a result of overlap */
    for (int i = 0; i < 4; i++) {
        switch (i) {
        case 1:
            if (!wnd->y)
                break;

            c_x = rects->x;
            c_h = rects->y;
            c_w = wnd->width - c_x;
            break;
        case 2:
            if ((rects->x + rects->w) >= wnd->width)
                break;

            c_x = rects->x + rects->w;
            c_y = rects->y;
            c_w = wnd->width - c_x;
            c_h = wnd->height - c_y;
            break;
        case 3:
            if ((rects->y + rects->h) >= wnd->height)
                break;

            c_x = 0;
            c_y = rects->y + rects->h;
            c_w = rects->x + rects->w;
            c_h = wnd->height - c_y;
            break;
        }

        if (c_w && c_h) {
            lwnd_draw_dbg_box(info, wnd->x + c_x, wnd->y + c_y, c_w, c_h, (fb_color_t) { { 0, 0xff, 0, 0xff } });

            create_and_link_lwndrect(wnd, c_x, c_y, c_w, c_h);
        }
    }

    return 0;
}

int lwnd_window_split(fb_info_t* info, lwnd_window_t* wnd)
{
    /* Preemptive rect clear */
    __window_clear_rects(wnd);

    /* Loop over all previous layers */
    for (lwnd_window_t* p = wnd->prev_layer; p != nullptr; p = p->prev_layer) {
        lwnd_wndrect_t overlap;

        /* Check if the fuckers overlap */
        if (lwnd_get_overlap_rect(wnd, p, &overlap))
            continue;

        /* TODO: Check if the window is even visible */

        /* Split the window based on the overlap(ping) rectangle(s) */
        __window_split(info, wnd, &overlap);

        lwnd_draw_dbg_box(info, wnd->x + overlap.x, wnd->y + overlap.y, overlap.w, overlap.h, (fb_color_t) { { 0, 0, 0xff, 0xff } });
    }
    return 0;
}
