#include "window.h"
#include "libk/stddef.h"
#include "mem/heap.h"
#include "mem/kmem_manager.h"
#include "mem/zalloc/zalloc.h"

lwnd_window_t* create_window(const char* title, u32 x, u32 y, u32 width, u32 height)
{
    lwnd_window_t* ret;

    ret = kmalloc(sizeof(*ret));

    if (!ret)
        return nullptr;

    memset(ret, 0, sizeof(*ret));

    ret->title = strdup(title);
    ret->x = x;
    ret->y = y;
    ret->width = width;
    ret->height = height;
    ret->rect_cache = create_zone_allocator(SMALL_PAGE_SIZE, sizeof(lwnd_wndrect_t), NULL);
    ret->flags = LWND_WINDOW_FLAG_NEED_UPDATE;

    /* Link a windows initial rect */
    create_and_link_lwndrect(&ret->rects, ret->rect_cache, 0, 0, width, height);

    return ret;
}

void destroy_window(lwnd_window_t* window)
{
    destroy_zone_allocator(window->rect_cache, false);

    kfree((void*)window->title);
    kfree(window);
}

/*!
 * @brief: Moves a single window across the screen
 *
 * Checks if this window had any intersections with other screens and prompts redraws of those windows
 */
int lwnd_window_move(lwnd_window_t* wnd, u32 newx, u32 newy)
{
    wnd->x = newx;
    wnd->y = newy;

    // for (lwnd_window_t* w = wnd; w != nullptr; w = wnd->next_layer)
    do {
        lwnd_window_update(wnd);

        wnd = wnd->next_layer;
    } while (wnd);
    return 0;
}
