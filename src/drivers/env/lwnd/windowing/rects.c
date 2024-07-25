#include "drivers/env/lwnd/windowing/stack.h"
#include "libk/stddef.h"
#include "logging/log.h"
#include "mem/zalloc/zalloc.h"
#include "window.h"

/*
 * Code that handles breaking up windows into smaller rectangles
 *
 * NOTE: Window coords are relative to their screen, but the window rect coords are relative to the
 * window position which they make up
 */

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

/*!
 * @brief: Get a rectangle covering the overlapping area between two rectangles
 */
static inline int lwndrects_get_overlap_rect(lwnd_wndrect_t* r0, lwnd_wndrect_t* r1, lwnd_wndrect_t* p_rect)
{
    u32 last_y = __max(r0->y, r1->y);
    u32 last_x = __max(r0->x, r1->x);
    u32 first_endx = __min(r1->x + r1->w, r0->x + r0->w);
    u32 first_endy = __min(r1->y + r1->h, r0->y + r0->h);

    if ((i32)(first_endx - last_x) <= 0)
        return -1;

    if ((i32)(first_endy - last_y) <= 0)
        return -1;

    if (p_rect) {
        p_rect->x = __abs(last_x - r0->x);
        p_rect->y = __abs(last_y - r0->y);

        p_rect->w = first_endx - last_x;
        p_rect->h = first_endy - last_y;
    }
    // TODO: ???
    return 0;
}

/*!
 * @brief: Get a rectangle covering the overlapping area between two windows
 *
 * NOTE: We could prevent code duplication by using lwndrects_get_overlap_rect here
 * and wrapping the windows in rects, but I'd rather not be pushing to extra cubes
 * to the stack for sauce, so cry about my code duplication
 *
 * TODO: Still fix this code dupe with a macro or generic function or sm
 */
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

    if (p_rect) {
        p_rect->x = __abs(last_x - w0->x);
        p_rect->y = __abs(last_y - w0->y);

        p_rect->w = first_endx - last_x;
        p_rect->h = first_endy - last_y;
    }
    // TODO: ???
    return 0;
}

lwnd_wndrect_t* create_and_link_lwndrect(lwnd_wndrect_t** rect_list, zone_allocator_t* cache, u32 x, u32 y, u32 w, u32 h)
{
    lwnd_wndrect_t* rect;

    rect = zalloc_fixed(cache);

    if (!rect)
        return nullptr;

    rect->next_part = *rect_list;
    rect->x = x;
    rect->y = y;
    rect->w = w;
    rect->h = h;
    rect->rect_changed = true;

    /* Link into the window */
    *rect_list = rect;

    return rect;
}

void __window_clear_rects(lwnd_window_t* wnd, lwnd_wndrect_t** pstart)
{
    lwnd_wndrect_t* next;

    while (*pstart) {
        next = (*pstart)->next_part;

        zfree_fixed(wnd->rect_cache, *pstart);

        *pstart = next;
    }
}

void window_clear_rects(lwnd_window_t* wnd)
{
    /* Create a new base rectangle if it does not yet exist (how?) */
    if (!wnd->rects) {
        create_and_link_lwndrect(&wnd->rects, wnd->rect_cache, 0, 0, wnd->width, wnd->height);
        return;
    }

    /* Reuse the first window */
    wnd->rects->x = 0;
    wnd->rects->y = 0;
    wnd->rects->w = wnd->width;
    wnd->rects->h = wnd->height;
    wnd->rects->rect_changed = true;

    __window_clear_rects(wnd, &wnd->rects->next_part);
}

/*!
 * @brief: Split a rectangle inside wnd, based on another overlapping rectangle
 *
 * NOTE: The coordinates of @overlap are relative to the coords of @rect
 *
 * @returns: The amount of splits done
 */
static u32 __rect_split(lwnd_wndrect_t** p_rects, lwnd_window_t* wnd, lwnd_wndrect_t* rect, lwnd_wndrect_t* overlap)
{
    u32 n_split = 0;
    /* Start at the rects topleft */
    u32 c_x = rect->x;
    u32 c_y = rect->y;
    u32 c_w = overlap->x;
    u32 c_h = overlap->y + overlap->h;
    lwnd_wndrect_t* new_rect;

    // KLOG_DBG("Trying to fix overlapping rect: (x: %d, y: %d)\n", overlap->x, overlap->y);

    /* The x and y coords of @overlap, relative to @wnd */
    u32 r_overlap_x = rect->x + overlap->x;
    u32 r_overlap_y = rect->y + overlap->y;

    /* There might be a max. of 4 rectangles we need to create as a result of overlap */
    for (int i = 0; i < 4; i++) {
        switch (i) {
        case 1:
            if (!r_overlap_y)
                break;

            /* X-coord needs to be relative to the window */
            c_x = r_overlap_x;
            c_h = overlap->y;
            c_w = rect->w - overlap->x;
            break;
        case 2:
            if ((overlap->x + overlap->w) >= rect->w)
                break;

            c_x = r_overlap_x + overlap->w;
            c_y = r_overlap_y;
            c_w = rect->w - (overlap->x + overlap->w);
            c_h = rect->h - overlap->y;
            break;
        case 3:
            if ((overlap->y + overlap->h) >= rect->h)
                break;

            c_x = rect->x;
            c_y = r_overlap_y + overlap->h;
            c_w = overlap->x + overlap->w;
            c_h = rect->h - (overlap->y + overlap->h);
            break;
        }

        if (c_w && c_h) {
            // lwnd_draw_dbg_box(info, wnd->x + c_x, wnd->y + c_y, c_w, c_h, (fb_color_t) { { 0, 0xff, 0, 0xff } });

            new_rect = create_and_link_lwndrect(p_rects, wnd->rect_cache, c_x, c_y, c_w, c_h);

            for (lwnd_wndrect_t* r = wnd->prev_rects; r && new_rect->rect_changed; r = r->next_part)
                if (r->x == new_rect->x && r->y == new_rect->y && r->w == new_rect->w && r->h == new_rect->h)
                    new_rect->rect_changed = false;

            n_split++;
        }
    }

    return n_split;
}

/*!
 * @brief: Replaces an overlapped rect from @wnd with new rects, as per @overlapping
 *
 * Convenient: When the entire rect is overlapped, there will be no splits, so the rectangle will be completely removed =)
 *
 * @wnd: The window in which the overlapping takes place
 * @overlapped: The overlapped rect from @wnd by @overlapping
 * @overlapping: The rectangle which overlaps with @overlapped
 */
static int __window_replace_overlapped_rect(lwnd_window_t* wnd, lwnd_wndrect_t** overlapped, lwnd_wndrect_t* overlapping)
{
    lwnd_wndrect_t* list;
    lwnd_wndrect_t* old_rect;

    if (!overlapped || !overlapping)
        return -KERR_INVAL;

    old_rect = *overlapped;

    if (!old_rect)
        return -KERR_NULL;

    /* Create a list */
    list = old_rect->next_part;

    /* Split the old rectangle */
    __rect_split(&list, wnd, old_rect, overlapping);

    *overlapped = list;

    /* Finish with freeing the old rectangle */
    zfree_fixed(wnd->rect_cache, old_rect);
    return 0;
}

static inline void try_replace_recursive_overlapping(lwnd_window_t* wnd, lwnd_wndrect_t* rect)
{
    lwnd_wndrect_t overlap = { 0 };
    lwnd_wndrect_t** r = &wnd->rects;

    do {
        memset(&overlap, 0, sizeof(overlap));

        for (; *r; r = &(*r)->next_part) {
            /* Skip rectangles that don't overlap */
            if (lwndrects_get_overlap_rect(*r, rect, &overlap) == 0)
                break;
        }

        /* Could not find any overlapping rect */
        if (!(*r))
            break;

        KLOG_DBG("Looping... overlap: x:%d,y:%d %d:%d\n", (*r)->x + overlap.x, (*r)->y + overlap.y, overlap.w, overlap.h);

        /* Smite it */
        __window_replace_overlapped_rect(wnd, r, &overlap);
    } while (true);
}

/*!
 * @brief: Split a window based on a rectangle marking overlap
 *
 * First checks if the @overlap rect is not also overlapping with other rects. If this is the case, every rect
 * that overlaps with @overlap will be recursively split. If not, we can just split the window like normal
 */
static int __window_split(lwnd_window_t* wnd, lwnd_wndrect_t** p_rects, lwnd_wndrect_t* overlap)
{
    /* If we did find overlapping, no need to do weird shit */
    try_replace_recursive_overlapping(wnd, overlap);

    return 0;
}

static void __window_set_prev_rects(lwnd_window_t* wnd)
{
    __window_clear_rects(wnd, &wnd->prev_rects);

    wnd->prev_rects = wnd->rects;
    wnd->rects = nullptr;

    create_and_link_lwndrect(&wnd->rects, wnd->rect_cache, 0, 0, wnd->width, wnd->height);
}

int lwnd_window_split(lwnd_wndstack_t* stack, lwnd_window_t* wnd, bool front_to_back)
{
    lwnd_wndrect_t overlap = { 0 };
    lwnd_window_t* c_window;
    const lwnd_window_t* end_window;

    /* Preemptive rect clear */
    __window_set_prev_rects(wnd);

    KLOG_DBG("Splitting window: (%d:%d)\n", wnd->width, wnd->height);

    if (front_to_back) {
        c_window = stack->top_window;
        end_window = wnd;
    } else {
        c_window = wnd->prev_layer;
        end_window = nullptr;
    }

    /* Loop over all previous layers */
    while (c_window && c_window != end_window) {

        /* Check if the fuckers overlap */
        if (lwnd_get_overlap_rect(wnd, c_window, &overlap))
            goto cycle;

        /* Full overlap, die */
        if (wnd->width == overlap.w && wnd->height == overlap.h) {
            __window_clear_rects(wnd, &wnd->rects);
            break;
        }

        /* Split the window based on the overlap(ping) rectangle(s) */
        __window_split(wnd, &wnd->rects, &overlap);

        // lwnd_draw_dbg_box(info, wnd->x + overlap.x, wnd->y + overlap.y, overlap.w, overlap.h, (fb_color_t) { { 0, 0, 0xff, 0xff } });

    cycle:
        /* Cycle */
        c_window = (front_to_back ? c_window->next_layer : c_window->prev_layer);
    }
    return 0;
}
