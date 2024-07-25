#include "window.h"
#include "dev/video/framebuffer.h"
#include "drivers/env/lwnd/display/screen.h"
#include "libk/stddef.h"
#include "mem/heap.h"
#include "mem/kmem_manager.h"
#include "mem/zalloc/zalloc.h"
#include "priv.h"

lwnd_window_t* create_window(proc_t* p, const char* title, u32 x, u32 y, u32 width, u32 height)
{
    lwnd_window_t* ret;

    ret = kmalloc(sizeof(*ret));

    if (!ret)
        return nullptr;

    memset(ret, 0, sizeof(*ret));

    ret->title = strdup(title);
    ret->proc = p;
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
 * @brief: Mark a window as needing an update
 */
void lwnd_window_update(lwnd_window_t* wnd)
{
    wnd->flags |= LWND_WINDOW_FLAG_NEED_UPDATE;
}

/*!
 * @brief: Mark a window as needing an update, without considiring the privious window state
 */
void lwnd_window_full_update(lwnd_window_t* wnd)
{
    lwnd_window_update(wnd);

    /* Clear the privous rectangle cache, in order to ensure this window */
    __window_clear_rects(wnd, &wnd->prev_rects);

    /* Also clear the default rect list */
    window_clear_rects(wnd);
}

/*!
 * @brief: Mark a window as not needing an update
 *
 * Called when the window update is finished
 */
void lwnd_window_clear_update(lwnd_window_t* wnd)
{
    wnd->flags &= ~LWND_WINDOW_FLAG_NEED_UPDATE;
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

    do {
        /* Just do a simple update */
        lwnd_window_update(wnd);

        wnd = wnd->next_layer;
    } while (wnd);
    return 0;
}

/*!
 * @brief: Update a single rect from a window
 *
 * Copies a part of the windows internal framebuffer to the screen
 *
 * TODO: Use 2D acceleration on the video device to bitblt
 */
static inline void __lwnd_redraw_rect(lwnd_window_t* wnd, lwnd_screen_t* screen, lwnd_wndrect_t* rect)
{
    u32 abs_rect_x;
    u32 abs_rect_y;
    fb_info_t* screen_info;
    fb_info_t* wnd_info;
    void* screen_offset;
    void* rect_offset;

    if (!wnd || !screen || !rect)
        return;

    screen_info = screen->fbinfo;
    wnd_info = wnd->this_fb;

    if (!screen_info || !screen_info->kernel_addr)
        return;

    if (!wnd_info || !wnd_info->kernel_addr)
        return;

    const uint32_t screen_bytes_per_pixel = screen_info->bpp >> 3;
    const uint32_t wnd_bytes_per_pixel = wnd_info->bpp >> 3;

    abs_rect_x = wnd->x + rect->x;
    abs_rect_y = wnd->y + rect->y;

    /* Get the starting offset on the screen where we need to draw */
    screen_offset = screen_info->kernel_addr + (void*)(u64)(abs_rect_y * screen_info->pitch + abs_rect_x * screen_bytes_per_pixel);
    /* Get the starting offset inside the windows framebuffer where we need to draw */
    rect_offset = wnd->this_fb->kernel_addr + (void*)(u64)(rect->y * wnd_info->pitch + rect->x * wnd_bytes_per_pixel);

    /*
     * Our manual slow bitblt xD
     */
    for (uint32_t i = 0; i < rect->h; i++) {
        for (uint32_t j = 0; j < rect->w; j++) {

            /* Don't overflow the screen */
            if ((abs_rect_x + j) >= screen_info->width)
                break;

            /* Don't overflow the window */
            if ((rect->x + j) >= wnd_info->width)
                break;

            *(uint32_t volatile*)(screen_offset + j * screen_bytes_per_pixel) = *(uint32_t*)(rect_offset + j * wnd_bytes_per_pixel);
        }

        if ((abs_rect_y + i) >= screen_info->height)
            break;

        if ((rect->y + i) >= wnd_info->height)
            break;

        screen_offset += screen_info->pitch;
        rect_offset += wnd_info->pitch;
    }
}

int lwnd_window_draw(lwnd_window_t* wnd, struct lwnd_screen* screen)
{
    for (lwnd_wndrect_t* r = wnd->rects; r; r = r->next_part)
        if (r->rect_changed)
            __lwnd_redraw_rect(wnd, screen, r);

    return 0;
}

/*!
 * @brief: Requests a framebuffer for this window to draw in
 */
int lwnd_window_request_fb(lwnd_window_t* wnd, lwnd_screen_t* screen)
{
    int error;
    fb_info_t* info;
    u8 bytes_pp;

    if (!wnd)
        return -KERR_INVAL;

    /* We already have a framebuffer */
    if (wnd->this_fb)
        return 0;

    info = kmalloc(sizeof(*info));

    if (!info)
        return -KERR_NOMEM;

    memcpy(info, screen->fbinfo, sizeof(*info));

    /* Quick compute of the bytepp */
    bytes_pp = info->bpp >> 3;

    /* Replace the rest of the data with our stuff */
    info->width = wnd->width;
    info->height = wnd->height;
    info->pitch = wnd->width * bytes_pp;
    info->size = ALIGN_UP(info->pitch * info->height, SMALL_PAGE_SIZE);
    info->addr = NULL;
    info->kernel_addr = NULL;
    /* We don't inherit the fb handle =/ */
    info->handle = 0;

    error = kmem_user_alloc_range((void**)&info->addr, wnd->proc, info->size, NULL, KMEM_FLAG_WRITABLE | KMEM_FLAG_NOEXECUTE);

    if (error)
        goto error_and_exit;

    /* Map to the kernel */
    error = kmem_map_to_kernel(&info->kernel_addr, info->addr, info->size, KERNEL_MAP_BASE, wnd->proc->m_root_pd.m_root, NULL, KMEM_FLAG_WRITABLE | KMEM_FLAG_NOEXECUTE);

    if (error)
        goto unmap_fb_and_exit;

    wnd->this_fb = info;
    return 0;

unmap_fb_and_exit:
    kmem_user_dealloc(wnd->proc, info->addr, info->size);
error_and_exit:
    kfree(info);
    return error;
}
