#include "screen.h"
#include "dev/video/framebuffer.h"
#include "drivers/env/lwnd/alloc.h"
#include "drivers/env/lwnd/event.h"
#include "drivers/env/lwnd/window.h"
#include "libk/data/bitmap.h"
#include "libk/flow/error.h"
#include "mem/kmem_manager.h"
#include "sync/mutex.h"

/*!
 * @brief: Allocate memory for a screen
 */
lwnd_screen_t* create_lwnd_screen(fb_info_t* fb, uint16_t max_window_count)
{
    lwnd_screen_t* ret;

    if (!fb)
        return nullptr;

    if (!max_window_count || max_window_count > LWND_SCREEN_MAX_WND_COUNT)
        max_window_count = LWND_SCREEN_MAX_WND_COUNT;

    /* We can simply allocate this fucker on the kernel heap, cuz we're cool like that */
    ret = allocate_lwnd_screen();

    if (!ret)
        return nullptr;

    memset(ret, 0, sizeof(*ret));

    ret->info = fb;
    ret->width = fb->width;
    ret->height = fb->height;
    ret->max_window_count = max_window_count;

    ret->event_lock = create_mutex(NULL);
    ret->draw_lock = create_mutex(NULL);

    /* Default quadrant size will be 16x16 */
    ret->quadrant_sz_log2 = 4;
    ret->quadrant_width = (u16)(ret->width >> ret->quadrant_sz_log2);
    /*
     * Create the quadrant bitmap
     * 0: Quadrant does not need to be updated
     * 1: Quadrant needs to be updated
     */
    ret->quadrant_bitmap = create_bitmap_ex((fb->width * fb->height) >> (2 * ret->quadrant_sz_log2), 0);
    ret->uid_bitmap = create_bitmap_ex(0xffff, 0);
    ret->window_stack_size = ALIGN_UP(max_window_count * sizeof(lwnd_window_t*), SMALL_PAGE_SIZE);

    ASSERT(!__kmem_kernel_alloc_range((void**)&ret->window_stack, ret->window_stack_size, NULL, KMEM_FLAG_KERNEL | KMEM_FLAG_WRITABLE));

    ASSERT_MSG(ret->quadrant_bitmap, "Could not make bitmap!");

    /* Make sure it is cleared */
    memset(ret->window_stack, 0, ret->window_stack_size);

    return ret;
}

/*!
 * @brief: Destroy memory owned by the screen
 */
void destroy_lwnd_screen(lwnd_screen_t* screen)
{
    if (!screen)
        return;

    destroy_mutex(screen->draw_lock);
    destroy_mutex(screen->event_lock);
    destroy_bitmap(screen->quadrant_bitmap);
    destroy_bitmap(screen->uid_bitmap);
    ASSERT(!__kmem_kernel_dealloc((uint64_t)screen->window_stack, screen->window_stack_size));
    deallocate_lwnd_screen(screen);
}

static int _generate_window_uid(lwnd_screen_t* screen, window_id_t* uid)
{
    kerror_t error;
    window_id_t id;

    error = bitmap_find_free(screen->uid_bitmap, (uint64_t*)&id);

    /* Could not find free entry */
    if ((error))
        return -1;

    /* Mark this bitch */
    bitmap_mark(screen->uid_bitmap, id);

    *uid = id;
    return 0;
}

static inline void _free_window_uid(lwnd_screen_t* screen, window_id_t uid)
{
    bitmap_unmark(screen->uid_bitmap, uid);
}

/*!
 * @brief: Register a window to the screen
 */
int lwnd_screen_register_window(lwnd_screen_t* screen, lwnd_window_t* window)
{
    int error;

    mutex_lock(screen->draw_lock);

    error = _generate_window_uid(screen, &window->uid);

    if (error)
        goto unlock_and_exit;

    window->screen = screen;

    screen->window_stack[screen->highest_wnd_idx] = window;
    window->stack_idx = screen->highest_wnd_idx;

    screen->window_count++;
    screen->highest_wnd_idx++;

unlock_and_exit:
    mutex_unlock(screen->draw_lock);
    return error;
}

/*!
 * @brief: Register a window at a specific id
 *
 * We can only register at places where there either already is a window, or
 * at the end of the current stack, erroring in a call to lwnd_screen_register_window
 *
 * FIXME: do we need to update windows anywhere?
 */
int lwnd_screen_register_window_at(lwnd_screen_t* screen, lwnd_window_t* window, window_id_t newid)
{
    window_id_t uid;
    lwnd_window_t *c, *p;

    mutex_lock(screen->draw_lock);

    if (_generate_window_uid(screen, &uid))
        goto unlock_and_exit;

    c = screen->window_stack[newid];
    p = c;

    /* Set the new window here */
    screen->window_stack[newid] = window;
    window->uid = uid;
    window->stack_idx = newid;

    /* Update highest_wnd_idx */
    if (newid > screen->highest_wnd_idx)
        screen->highest_wnd_idx = newid;

    newid++;
    c = screen->window_stack[newid];

    while (c) {

        /* Shift the previous window forward */
        screen->window_stack[newid] = p;
        screen->window_stack[newid]->stack_idx = newid;

        /* Cache the current entry as the previous */
        p = c;

        /* Cycle to the next window */
        newid++;
        c = screen->window_stack[newid];
    }

    screen->window_count++;

unlock_and_exit:
    mutex_unlock(screen->draw_lock);
    return 0;
}

/*!
 * @brief: Remove a window from a screen
 *
 * This shifts the windows after it back by one and updates the windowids
 */
int lwnd_screen_unregister_window(lwnd_screen_t* screen, lwnd_window_t* window)
{
    int error;

    if (!screen || !screen->window_count)
        return -1;

    if (window->stack_idx == LWND_INVALID_ID)
        return -1;

    /* Check both range and validity */
    if (!lwnd_window_has_valid_index(window))
        return -1;

    error = 0;

    mutex_lock(screen->draw_lock);

    /* Make sure the window area is cleared for draw */
    lwnd_clear(window);

    for (uint32_t i = 0; i < window->stack_idx; i++)
        lwnd_window_update(screen->window_stack[i], false);

    /* Free up this uid */
    _free_window_uid(screen, window->uid);

    /* We know window is valid and contained within the stack */
    screen->window_stack[window->stack_idx] = nullptr;

    /* Solve weird shit */
    for (uint32_t i = window->stack_idx; i < screen->highest_wnd_idx; i++) {
        if (screen->window_stack[i + 1])
            screen->window_stack[i + 1]->stack_idx = i;
        screen->window_stack[i] = screen->window_stack[i + 1];
    }

    if ((window->stack_idx + 1) == screen->highest_wnd_idx)
        /* Scan down the stack */
        while (screen->highest_wnd_idx && !screen->window_stack[screen->highest_wnd_idx - 1])
            screen->highest_wnd_idx--;

    window->stack_idx = LWND_INVALID_ID;
    window->uid = LWND_INVALID_ID;
    window->screen = nullptr;

    screen->window_count--;

    mutex_unlock(screen->draw_lock);

    return error;
}

/*!
 * @brief: Scan the entire window stack to find a window by label
 */
lwnd_window_t* lwnd_screen_get_window_by_label(lwnd_screen_t* screen, const char* label)
{
    lwnd_window_t* c;

    for (uint32_t i = 0; i < screen->highest_wnd_idx; i++) {
        c = screen->window_stack[i];

        if (!c || strcmp(c->label, label) == 0)
            break;

        c = nullptr;
    }

    return c;
}

static inline bool get_quadrant_for_pixel(lwnd_screen_t* screen, u32 x, u32 y)
{
    u32 q_x = x >> screen->quadrant_sz_log2;
    u32 q_y = y >> screen->quadrant_sz_log2;

    return bitmap_isset(screen->quadrant_bitmap, q_y * screen->quadrant_width + q_x);
}

/*!
 * @brief: Draw a windows framebuffer to the screen
 *
 * TODO: can we integrate 2D accelleration here to make this at least a little fast?
 */
int lwnd_screen_draw_window(lwnd_window_t* window)
{
    lwnd_screen_t* this;
    fb_color_t* current_color;

    if (!window)
        return -1;

    this = window->screen;

    uint32_t current_offset = this->info->pitch * window->y + window->x * this->info->bpp / 8;
    const uint32_t bpp = this->info->bpp;
    const uint32_t increment = (bpp >> 3);

    for (u32 y = 0; y < window->height; y++) {
        for (u32 x = 0; x < (window->width * increment); x += increment) {
            if ((window->x + x) >= (this->width * increment))
                break;

            if (get_quadrant_for_pixel(this, x, y))
        }
    }

    for (uint32_t i = 0; i < window->height; i++) {
        for (uint32_t j = 0; j < window->width; j++) {

            if (window->x + j >= this->width)
                break;

            if (bitmap_isset(this->quadrant_bitmap, (window->y + i) * this->width + (window->x + j)))
                continue;

            current_color = get_color_at(window, j, i);

            // if (this->info->colors.alpha.length_bits)
            //*(uint8_t volatile*)(this->info->kernel_addr + current_offset + j * increment + (this->info->colors.alpha.offset_bits / 8)) = current_color->components.a;

            *(uint8_t volatile*)(this->info->kernel_addr + current_offset + j * increment + (this->info->colors.red.offset_bits / 8)) = current_color->components.r;
            *(uint8_t volatile*)(this->info->kernel_addr + current_offset + j * increment + (this->info->colors.green.offset_bits / 8)) = current_color->components.g;
            *(uint8_t volatile*)(this->info->kernel_addr + current_offset + j * increment + (this->info->colors.blue.offset_bits / 8)) = current_color->components.b;

            bitmap_mark(this->quadrant_bitmap, (window->y + i) * this->width + (window->x + j));
        }

        if (window->y + i >= this->height)
            break;

        current_offset += this->info->pitch;
    }

    return 0;
}

/*!
 * @brief: Add an lwnd_event to the end of a screens event chain
 *
 * Caller musn't take the ->event_lock of @screen
 */
int lwnd_screen_add_event(lwnd_screen_t* screen, lwnd_event_t* event)
{
    lwnd_event_t** current;

    current = &screen->events;

    mutex_lock(screen->event_lock);

    while (*current) {
        if (*current == event)
            break;

        current = &(*current)->next;
    }

    if (*current) {
        mutex_unlock(screen->event_lock);
        return -1;
    }

    *current = event;

    mutex_unlock(screen->event_lock);
    return 0;
}

/*!
 * @brief: Takes the head event from the screens event chain
 *
 * Caller musn't take the ->event_lock of @screen
 */
lwnd_event_t* lwnd_screen_take_event(lwnd_screen_t* screen)
{
    lwnd_event_t* ret;

    mutex_lock(screen->event_lock);

    ret = screen->events;

    if (!ret) {
        mutex_unlock(screen->event_lock);
        return nullptr;
    }

    screen->events = ret->next;

    mutex_unlock(screen->event_lock);
    return ret;
}

/*!
 * @brief: Handle a single event from the screen
 */
void lwnd_screen_handle_event(lwnd_screen_t* screen, lwnd_event_t* event)
{
    lwnd_window_t* window;

    switch (event->type) {
    case LWND_EVENT_TYPE_MOVE: {
        window = lwnd_screen_get_window(screen, event->window_id);

        if (!window)
            break;

        lwnd_window_move(window, event->move.new_x, event->move.new_y);
        break;
    }
    default:
        break;
    }
}

/*!
 * @brief: Handle every event in the screens chain
 *
 * TODO: event compositing
 */
void lwnd_screen_handle_events(lwnd_screen_t* screen)
{
    lwnd_event_t* current_event;
    lwnd_event_t* next_event;

    if (!screen || !screen->events)
        return;

    current_event = lwnd_screen_take_event(screen);

    while (current_event) {
        next_event = lwnd_screen_take_event(screen);

        /* Consecutive events acting on the same window? */
        while (next_event && next_event->type == current_event->type && next_event->window_id == current_event->window_id) {
            /* Destroy this one preemptively, since it does not really matter to us */
            destroy_lwnd_event(current_event);

            /* Set the current event to the next one */
            current_event = next_event;

            /* Cycle further */
            next_event = lwnd_screen_take_event(screen);
        }

        lwnd_screen_handle_event(screen, current_event);

        destroy_lwnd_event(current_event);

        current_event = next_event;
    }
}
