#include "stack.h"
#include "dev/video/framebuffer.h"
#include "drivers/env/lwnd/windowing/window.h"
#include "libk/data/hashmap.h"
#include "libk/flow/error.h"
#include "libk/stddef.h"
#include "mem/heap.h"
#include "sys/types.h"
#include <libk/string.h>
#include <system/asm_specifics.h>

lwnd_wndstack_t* create_lwnd_wndstack(u16 max_n_wnd, fb_info_t* info)
{
    lwnd_wndstack_t* ret;

    ret = kmalloc(sizeof(*ret));

    if (!ret)
        return nullptr;

    memset(ret, 0, sizeof(*ret));

    ret->top_window = nullptr;
    ret->bottom_window = nullptr;
    ret->max_n_wnd = max_n_wnd;
    ret->fbinfo = info;
    ret->wnd_map = create_hashmap(max_n_wnd, NULL);

    ret->background_window = create_window("__background", 0, 0, info->width, info->height);

    return ret;
}

void destroy_lwnd_wndstack(lwnd_wndstack_t* stack)
{
    destroy_hashmap(stack->wnd_map);
    kfree(stack);
}

static void __tmp_draw_fragmented_windo(fb_info_t* info, lwnd_window_t* window, fb_color_t clr)
{
    for (lwnd_wndrect_t* r = window->rects; r; r = r->next_part)
        if (r->rect_changed)
            generic_draw_rect(info, window->x + r->x, window->y + r->y, r->w, r->h, clr);
}

int lwnd_wndstack_update_background(lwnd_wndstack_t* stack)
{
    if (!lwnd_window_should_update(stack->background_window))
        return 0;

    stack->background_window->prev_layer = stack->bottom_window;

    /* Split the background window based on it's top most windows */
    lwnd_window_split(stack, stack->background_window, true);

    __tmp_draw_fragmented_windo(stack->fbinfo, stack->background_window, (fb_color_t) { { 0x1f, 0x1f, 0x1f, 0xff } });

    /* Update the 'window' itself */
    stack->background_window->flags &= ~LWND_WINDOW_FLAG_NEED_UPDATE;
    return 0;
}

static void _wndstack_link_window(lwnd_wndstack_t* stack, lwnd_window_t* wnd)
{
    /* Do the link */
    wnd->next_layer = stack->top_window;
    wnd->prev_layer = nullptr;

    if (stack->top_window)
        stack->top_window->prev_layer = wnd;

    stack->top_window = wnd;

    if (!stack->bottom_window)
        stack->bottom_window = wnd;
}

/*!
 * @brief: Add a window to the window stack
 *
 *
 */
int wndstack_add_window(lwnd_wndstack_t* stack, struct lwnd_window* wnd)
{
    int error;

    /* Check bounds here */
    if (stack->wnd_map->m_size >= stack->max_n_wnd)
        return -KERR_NOMEM;

    /* Try to add to the hashmap first */
    if ((error = hashmap_put(stack->wnd_map, (hashmap_key_t)wnd->title, wnd)))
        return error;

    /* do the link */
    _wndstack_link_window(stack, wnd);
    return 0;
}

static lwnd_window_t** _get_lwndstack_window(lwnd_wndstack_t* stack, lwnd_window_t* wnd)
{
    lwnd_window_t** walker;

    for (walker = &stack->top_window; *walker; walker = &(*walker)->next_layer)
        if (*walker == wnd)
            break;

    if (!(*walker))
        return nullptr;

    return walker;
}

/*!
 * @brief: Unlink a window while keeping the bottom_window pointer in mind
 */
static inline void _lwndstack_unlink_window(lwnd_wndstack_t* stack, lwnd_window_t** p_wnd, lwnd_window_t* wnd)
{
    lwnd_window_t* prev;

    /* Break the link */
    *p_wnd = wnd->next_layer;

    /* Resolve a pointer to the previous window */
    prev = wnd->prev_layer;

    /* Also make sure the previous chain is kept intact */
    if (wnd->next_layer)
        wnd->next_layer->prev_layer = prev;

    /* Set the bottom window, if we're removing the current bottom window */
    if (stack->bottom_window == wnd)
        stack->bottom_window = prev;
}

int wndstack_remove_window(lwnd_wndstack_t* stack, struct lwnd_window* wnd)
{
    lwnd_window_t** p_wnd = _get_lwndstack_window(stack, wnd);

    if (!p_wnd)
        return -KERR_NOT_FOUND;

    /* Do the unlink */
    _lwndstack_unlink_window(stack, p_wnd, wnd);

    /* Also remove from the hashmap */
    ASSERT_MSG(hashmap_remove(stack->wnd_map, (hashmap_key_t)wnd->title) == 0, "Tried to remove a window that was found in the layer list, but not in the hashmap!");

    return 0;
}

int wndstack_focus_window(lwnd_wndstack_t* stack, struct lwnd_window* wnd)
{
    lwnd_window_t** p_wnd = _get_lwndstack_window(stack, wnd);

    if (!p_wnd)
        return -KERR_NOT_FOUND;

    /* Do the unlink */
    _lwndstack_unlink_window(stack, p_wnd, wnd);

    /* Relink at the start */
    _wndstack_link_window(stack, wnd);

    return 0;
}

int wndstack_cycle_windows(lwnd_wndstack_t* stack)
{
    lwnd_window_t* old_top;
    lwnd_window_t* new_top;

    old_top = stack->top_window;

    /* No windows in the stack */
    if (!old_top || !stack->bottom_window)
        return KERR_NOT_FOUND;

    new_top = old_top->next_layer;

    /* Only one window, can't cycle */
    if (!new_top)
        return KERR_NOT_FOUND;

    /* The bottom of the stack is now going to be the old top window */
    stack->bottom_window->next_layer = old_top;
    old_top->prev_layer = stack->bottom_window;
    old_top->next_layer = nullptr;

    /* Put the old top window at the bottom */
    stack->bottom_window = old_top;

    /* The new top isn't going to have a next layer */
    new_top->prev_layer = nullptr;

    /* And finish off the stack */
    stack->top_window = new_top;

    /* Make sure all underlying windows are updated */
    do {
        lwnd_window_update(new_top);
        new_top = new_top->next_layer;
    } while (new_top);

    return 0;
}
