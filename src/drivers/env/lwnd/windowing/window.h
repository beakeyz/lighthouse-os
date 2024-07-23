#ifndef __LWND_WINDOWING_WINDOW__
#define __LWND_WINDOWING_WINDOW__

#include "dev/video/framebuffer.h"
#include "libk/stddef.h"
#include "mem/zalloc/zalloc.h"

#define LWND_WINDOW_FLAG_NEED_UPDATE 0x00000001

struct lwnd_wndstack;

/*
 * Rectangle part of a window
 *
 * Used to break the window into smaller parts that need to be
 * updated, when there is overlap between multiple windows
 */
typedef struct lwnd_wndrect {
    /* These are relative to the window */
    u32 x, y;
    u32 w, h;

    struct lwnd_wndrect* next_part;
} lwnd_wndrect_t;

typedef struct lwnd_window {
    const char* title;

    u32 x, y;
    u32 width, height;

    /* Index this window has inside it's window stack */
    i32 stack_idx;
    u32 flags;

    /* Small cache to store the local rectancles of this window */
    zone_allocator_t* rect_cache;

    /* The window under this window xD */
    struct lwnd_window* next_layer;
    struct lwnd_window* prev_layer;

    /*
     * The different rectangles that make up this window
     * Null if the window us unbroken
     */
    lwnd_wndrect_t* rects;
} lwnd_window_t;

static inline void lwnd_window_update(lwnd_window_t* wnd)
{
    wnd->flags |= LWND_WINDOW_FLAG_NEED_UPDATE;
}

static inline bool lwnd_window_should_update(lwnd_window_t* wnd)
{
    return ((wnd->flags & LWND_WINDOW_FLAG_NEED_UPDATE) == LWND_WINDOW_FLAG_NEED_UPDATE);
}

static inline bool lwnd_window_is_inside_window(lwnd_window_t* top, lwnd_window_t* bottom)
{
    return (top->x >= bottom->x && top->y >= bottom->y && (top->x + top->width) <= (bottom->x + bottom->width) && (top->y + top->height) <= (bottom->y + bottom->height));
}

lwnd_window_t* create_window(const char* title, u32 x, u32 y, u32 width, u32 height);
void destroy_window(lwnd_window_t* window);

extern lwnd_wndrect_t* create_and_link_lwndrect(lwnd_wndrect_t** rect_list, zone_allocator_t* cache, u32 x, u32 y, u32 w, u32 h);

// Uses fb_info or debug
extern int lwnd_window_split(fb_info_t* info, lwnd_window_t* wnd);
int lwnd_window_move(lwnd_window_t* wnd, u32 newx, u32 newy);

#endif // !__LWND_WINDOWING_WINDOW__
