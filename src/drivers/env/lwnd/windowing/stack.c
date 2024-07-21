#include "stack.h"
#include "drivers/env/lwnd/windowing/window.h"
#include "libk/data/hashmap.h"
#include "libk/flow/error.h"
#include "libk/stddef.h"
#include "mem/heap.h"
#include "sys/types.h"
#include <libk/string.h>
#include <system/asm_specifics.h>

lwnd_wndstack_t* create_lwnd_wndstack(u16 max_n_wnd)
{
    lwnd_wndstack_t* ret;

    ret = kmalloc(sizeof(*ret));

    if (!ret)
        return nullptr;

    memset(ret, 0, sizeof(*ret));

    ret->top_window = nullptr;
    ret->bottom_window = nullptr;
    ret->max_n_wnd = max_n_wnd;
    ret->wnd_map = create_hashmap(max_n_wnd, NULL);

    return ret;
}

void destroy_lwnd_wndstack(lwnd_wndstack_t* stack)
{
    destroy_hashmap(stack->wnd_map);
    kfree(stack);
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
