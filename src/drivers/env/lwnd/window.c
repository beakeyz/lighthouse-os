#include "window.h"
#include "dev/video/framebuffer.h"
#include "drivers/env/lwnd/alloc.h"
#include "drivers/env/lwnd/screen.h"
#include "libk/data/bitmap.h"
#include "libk/flow/error.h"
#include "mem/kmem_manager.h"
#include "sync/mutex.h"

/*!
 * @brief: Construct window memory
 */
lwnd_window_t* create_lwnd_window(struct lwnd_screen* screen, uint32_t startx, uint32_t starty, uint32_t width, uint32_t height, uint32_t flags, window_type_t type, void* client)
{
    lwnd_window_t* ret;

    ret = allocate_lwnd_window();

    if (!ret)
        return nullptr;

    memset(ret, 0, sizeof(*ret));

    ret->x = startx;
    ret->y = starty;
    ret->width = width;
    ret->height = height;

    ret->type = type;
    ret->client.raw = client;

    ret->flags = flags | LWND_WNDW_NEEDS_SYNC;

    ret->lock = create_mutex(NULL);

    /* Register to the screen to obtain an id */
    lwnd_screen_register_window(screen, ret);

    return ret;
}

/*!
 * @brief: Destroy a screen and memory it owns
 */
void destroy_lwnd_window(lwnd_window_t* window)
{
    ASSERT_MSG(!window->screen, "Tried to destroy a window which is still registered");

    ASSERT(!__kmem_kernel_dealloc((vaddr_t)window->fb_ptr, window->fb_size));

    destroy_mutex(window->lock);
    deallocate_lwnd_window(window);
}

/*!
 * @brief: Add a keyevent to the keybuffer of a window
 */
int lwnd_save_keyevent(lwnd_window_t* window, hid_event_t* ctx)
{
    window->key_buffer[window->key_buffer_write_idx++] = *ctx;
    window->key_buffer_write_idx %= LWND_WINDOW_KEYBUFFER_CAPACITY;
    return 0;
}

int lwnd_load_keyevent(lwnd_window_t* window, hid_event_t* ctx)
{
    if (!window || window->key_buffer_read_idx == window->key_buffer_write_idx)
        return -1;

    *ctx = window->key_buffer[window->key_buffer_read_idx++];
    window->key_buffer_read_idx %= LWND_WINDOW_KEYBUFFER_CAPACITY;
    return 0;
}

static inline int lwnd_allocate_fb_range(lwnd_window_t* window, size_t size)
{
    paddr_t physical;
    proc_t* client;

    if (!window || !size)
        return -1;

    client = window->client.proc;

    window->user_fb_ptr = nullptr;
    window->user_real_fb_ptr = nullptr;
    window->fb_ptr = nullptr;

    ASSERT(!__kmem_kernel_alloc_range((void**)&window->fb_ptr, size, NULL, KMEM_FLAG_WRITABLE));

    if (window->type == LWND_TYPE_PROCESS && window->client.proc) {

        physical = kmem_to_phys(nullptr, (uintptr_t)window->fb_ptr);

        ASSERT(!kmem_user_alloc((void**)&window->user_real_fb_ptr, client, physical, size, NULL, KMEM_FLAG_WRITABLE));
    }

    memset(window->fb_ptr, 0, size);

    return 0;
}

/*!
 * @brief: Try to get a framebuffer for this window
 *
 * NOTE: windows may render in two modes
 * these are:
 * - framebuffer mode: client gets a raw framebuffer it can draw to
 * - widget mode: client can attach widgets that are predrawn from a
 *                userspace library
 */
int lwnd_request_framebuffer(lwnd_window_t* window)
{
    size_t new_size;
    size_t old_size;
    void* old_fb;

    if (!window)
        return -1;

    new_size = ALIGN_UP(window->width * (window->height) * sizeof(fb_color_t), SMALL_PAGE_SIZE);

    if (!window->fb_ptr)
        goto create_new_framebuffer;

    if (window->fb_size == new_size)
        goto exit_ok;

    /* Cache old fb */
    old_fb = window->fb_ptr;
    old_size = window->fb_size;

    /* Allocate new fb with new size */
    window->fb_size = new_size;
    (void)lwnd_allocate_fb_range(window, new_size);

    /* Queue redraw into the new fb */
    window->flags |= LWND_WNDW_NEEDS_SYNC;

    /* Deallocate old framebuffer and return */
    ASSERT(!__kmem_kernel_dealloc((vaddr_t)old_fb, old_size));
    return 0;

create_new_framebuffer:
    window->fb_size = new_size;
    (void)lwnd_allocate_fb_range(window, new_size);
exit_ok:
    return 0;
}

/*!
 * @brief: Try to draw a window to the screen
 *
 * We either copy the windows framebuffer, or we walk its list of widgets
 * and draw those or we let the videocard go brrr
 */
int lwnd_draw(lwnd_window_t* window)
{
    int error;

    if (!window || !window->ops || !window->ops->f_draw)
        return -1;

    /* Don't need to do anything */
    if ((window->flags & LWND_WNDW_NEEDS_SYNC) != LWND_WNDW_NEEDS_SYNC) {
        if ((window->flags & LWND_WNDW_NEEDS_REPAINT) == LWND_WNDW_NEEDS_REPAINT)
            goto do_repaint;

        return 0;
    }

    /* Get pixels */
    error = window->ops->f_draw(window);

    if (error)
        return error;

    /* Clear the flag, since we have just synced our window to the screen */
    window->flags &= ~LWND_WNDW_NEEDS_SYNC;

do_repaint:
    window->flags &= ~LWND_WNDW_NEEDS_REPAINT;
    /* Put pixels */
    return lwnd_screen_draw_window(window);
}

/*!
 * @brief: Clear the pixels of a window so they get repainted
 */
int lwnd_clear(lwnd_window_t* window)
{
    lwnd_screen_t* this;

    if (!window || !window->screen)
        return -1;

    this = window->screen;

    for (uint32_t i = 0; i < window->height; i++)
        bitmap_unmark_range(this->pixel_bitmap, (window->y + i) * this->width + window->x, this->width);

    return 0;
}

static inline void lwnd_window_check_position(lwnd_window_t* window, int* new_x, int* new_y)
{
    /* Check x bound */
    if (*new_x < 0)
        *new_x = 0;
    else if (*new_x >= window->screen->width)
        *new_x = window->screen->width - 1;

    /* Check y bound */
    if (*new_y < 0)
        *new_y = 0;
    else if (*new_y >= window->screen->height)
        *new_y = window->screen->height - 1;
}

/*!
 * @brief: Prompt a window move
 *
 * FIXME: in stead of always clearing twice, we can check for an intersection
 */
int lwnd_window_move(lwnd_window_t* window, uint32_t new_x, uint32_t new_y)
{
    if (!window || !window->screen)
        return -1;

    /* Prompt a focus */
    if (!lwnd_window_is_top_window(window))
        lwnd_window_focus(window);

    lwnd_window_check_position(window, (int*)&new_x, (int*)&new_y);

    mutex_lock(window->lock);

    /* Clear old window pixels */
    lwnd_clear(window);

    /* Set new coords */
    window->x = new_x;
    window->y = new_y;

    /* Clear new pixels so they get painted correctly */
    lwnd_clear(window);

    /* Mark a repaint, since the window contents didn't change */
    window->flags |= LWND_WNDW_NEEDS_REPAINT;

    mutex_unlock(window->lock);
    return 0;
}

/*!
 * @brief: Reorder a window to be on top of the stack
 */
int lwnd_window_focus(lwnd_window_t* window)
{
    lwnd_screen_t* s;
    lwnd_window_t* w;

    if (!window || !window->screen)
        return -1;

    w = lwnd_screen_get_window(window->screen, window->uid);

    if (w != window)
        return -1;

    s = w->screen;

    /* Completely remove */
    if (lwnd_screen_unregister_window(s, w))
        return -1;

    /* Add at the end */
    return lwnd_screen_register_window(s, w);
}

int lwnd_window_update(lwnd_window_t* window, bool should_clear)
{
    if (!window)
        return -1;

    if (should_clear)
        lwnd_clear(window);
    window->flags |= LWND_WNDW_NEEDS_SYNC;
    return 0;
}
