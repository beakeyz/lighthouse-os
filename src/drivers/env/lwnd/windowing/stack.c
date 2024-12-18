#include "stack.h"
#include "drivers/env/lwnd/display/screen.h"
#include "drivers/env/lwnd/windowing/window.h"
#include "libk/data/hashmap.h"
#include "libk/flow/error.h"
#include "libk/stddef.h"
#include "logging/log.h"
#include "mem/heap.h"
#include "sync/mutex.h"
#include "sys/types.h"
#include <libk/string.h>
#include <system/asm_specifics.h>

lwnd_wndstack_t* create_lwnd_wndstack(u16 max_n_wnd, lwnd_screen_t* screen)
{
    lwnd_wndstack_t* ret;

    ret = kmalloc(sizeof(*ret));

    if (!ret)
        return nullptr;

    memset(ret, 0, sizeof(*ret));

    ret->top_window = nullptr;
    ret->bottom_window = nullptr;
    ret->max_n_wnd = max_n_wnd;
    ret->screen = screen;
    ret->background_window = screen->background_window;
    ret->wnd_map = create_hashmap(max_n_wnd, NULL);
    ret->order_lock = create_mutex(NULL);
    ret->map_lock = create_mutex(NULL);

    return ret;
}

void destroy_lwnd_wndstack(lwnd_wndstack_t* stack)
{
    destroy_hashmap(stack->wnd_map);
    destroy_mutex(stack->order_lock);
    destroy_mutex(stack->map_lock);
    kfree(stack);
}

static inline void __wndstack_do_update_background(lwnd_wndstack_t* stack)
{
    /* Mark background as needing an update */
    lwnd_window_full_update_screen(stack->background_window, stack->screen);

    /* Do the update */
    lwnd_wndstack_update_background(stack);
}

int lwnd_wndstack_update_background(lwnd_wndstack_t* stack)
{
    if (!lwnd_window_should_update(stack->background_window))
        return 0;

    /* Temporarily hook into the stack */
    stack->background_window->prev_layer = stack->bottom_window;

    /* Split the background window based on it's top most windows */
    lwnd_window_split(stack, stack->background_window, true);

    /* Actualy draw the window */
    lwnd_window_draw(stack->background_window, stack->screen);

    /* Update the 'window' itself */
    stack->background_window->flags &= ~LWND_WINDOW_FLAG_NEED_UPDATE;
    stack->background_window->prev_layer = NULL;
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

struct lwnd_window* wndstack_find_window(lwnd_wndstack_t* stack, const char* title)
{
    lwnd_window_t* ret;

    mutex_lock(stack->map_lock);

    ret = hashmap_get(stack->wnd_map, (hashmap_key_t)title);

    mutex_unlock(stack->map_lock);

    return ret;
}

/*!
 * @brief: Add a window to the window stack
 *
 *
 */
int wndstack_add_window(lwnd_wndstack_t* stack, struct lwnd_window* wnd)
{
    int error;

    if (!stack || !wnd)
        return -KERR_INVAL;

    /* Check bounds here */
    if (stack->wnd_map->m_size >= stack->max_n_wnd)
        return -KERR_NOMEM;

    mutex_lock(stack->map_lock);
    mutex_lock(stack->order_lock);

    KLOG_DBG("Adding window: %s\n", wnd->title);

    /* Try to add to the hashmap first */
    if ((error = hashmap_put(stack->wnd_map, (hashmap_key_t)wnd->title, wnd)))
        goto unlock_and_exit;

    /* do the link */
    _wndstack_link_window(stack, wnd);

unlock_and_exit:
    mutex_unlock(stack->order_lock);
    mutex_unlock(stack->map_lock);
    return error;
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

/*!
 * @brief: Removes a window from a stack
 *
 * Caller needs to have both the map lock and the order lock
 */
int wndstack_remove_window(lwnd_wndstack_t* stack, struct lwnd_window* wnd)
{
    lwnd_window_t** p_wnd = _get_lwndstack_window(stack, wnd);

    if (!p_wnd)
        return -KERR_NOT_FOUND;

    /* Do the unlink */
    _lwndstack_unlink_window(stack, p_wnd, wnd);

    KLOG_DBG("Removing window: %s\n", wnd->title);

    /* Also remove from the hashmap */
    ASSERT_MSG(hashmap_remove(stack->wnd_map, (hashmap_key_t)wnd->title) != nullptr, "Tried to remove a window that was found in the layer list, but not in the hashmap!");

    /* Update the background window */
    __wndstack_do_update_background(stack);
    return 0;
}

int wndstack_remove_window_ex(lwnd_wndstack_t* stack, const char* title, struct lwnd_window** pwnd)
{
    lwnd_window_t* wnd;
    lwnd_window_t** p_wnd;

    mutex_lock(stack->map_lock);
    mutex_lock(stack->order_lock);

    /* Get the window pointer */
    wnd = hashmap_get(stack->wnd_map, (hashmap_key_t)title);

    /* Get the window slot */
    p_wnd = _get_lwndstack_window(stack, wnd);

    if (!p_wnd)
        return -KERR_NOT_FOUND;

    /* Do the unlink */
    _lwndstack_unlink_window(stack, p_wnd, wnd);

    KLOG_DBG("Removing window: %s\n", wnd->title);

    /* Also remove from the hashmap */
    ASSERT_MSG(hashmap_remove(stack->wnd_map, (hashmap_key_t)wnd->title) != nullptr, "Tried to remove a window that was found in the layer list, but not in the hashmap!");

    /* Update the background window */
    __wndstack_do_update_background(stack);

    /* Export the bitch */
    if (pwnd)
        *pwnd = wnd;

    mutex_unlock(stack->order_lock);
    mutex_unlock(stack->map_lock);
    return 0;
}

int wndstack_focus_window(lwnd_wndstack_t* stack, struct lwnd_window* wnd)
{
    lwnd_window_t** p_wnd = _get_lwndstack_window(stack, wnd);

    if (!p_wnd)
        return -KERR_NOT_FOUND;

    mutex_lock(stack->order_lock);

    /* Do the unlink */
    _lwndstack_unlink_window(stack, p_wnd, wnd);

    /* Relink at the start */
    _wndstack_link_window(stack, wnd);

    mutex_unlock(stack->order_lock);
    return 0;
}

int wndstack_cycle_windows(lwnd_wndstack_t* stack)
{
    int error = KERR_NOT_FOUND;
    lwnd_window_t* old_top;
    lwnd_window_t* new_top;

    mutex_lock(stack->order_lock);

    old_top = stack->top_window;

    /* No windows in the stack */
    if (!old_top || !stack->bottom_window)
        goto unlock_and_exit;

    new_top = old_top->next_layer;

    /* Only one window, can't cycle */
    if (!new_top)
        goto unlock_and_exit;

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
        /* Need to do a full update, since no sizes changed, only drawing order */
        lwnd_window_full_update_screen(new_top, stack->screen);
        new_top = new_top->next_layer;
    } while (new_top);

    /* Reset error num */
    error = 0;

unlock_and_exit:
    mutex_unlock(stack->order_lock);
    return error;
}
