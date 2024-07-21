#ifndef __LWND_WINDOWING_WINDOW__
#define __LWND_WINDOWING_WINDOW__

#include "dev/video/framebuffer.h"
#include "libk/stddef.h"

#define LWND_WINDOW_FLAG_NEED_UPDATE 0x00000001

struct lwnd_wndrect;

typedef struct lwnd_window {
    const char* title;

    u32 x, y;
    u32 width, height;

    /* Index this window has inside it's window stack */
    i32 stack_idx;
    u32 flags;

    /* The window under this window xD */
    struct lwnd_window* next_layer;
    struct lwnd_window* prev_layer;

    /*
     * The different rectangles that make up this window
     * Null if the window us unbroken
     */
    struct lwnd_wndrect* rects;
} lwnd_window_t;

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

static inline void lwnd_window_update(lwnd_window_t* wnd)
{
    wnd->flags |= LWND_WINDOW_FLAG_NEED_UPDATE;
}

static inline bool lwnd_window_should_update(lwnd_window_t* wnd)
{
    return ((wnd->flags & LWND_WINDOW_FLAG_NEED_UPDATE) == LWND_WINDOW_FLAG_NEED_UPDATE);
}

lwnd_window_t* create_window(const char* title, u32 x, u32 y, u32 width, u32 height);
void destroy_window(lwnd_window_t* window);

// Uses fb_info or debug
extern int lwnd_window_split(fb_info_t* info, lwnd_window_t* wnd);
int lwnd_window_move(lwnd_window_t* wnd, u32 newx, u32 newy);

#endif // !__LWND_WINDOWING_WINDOW__
