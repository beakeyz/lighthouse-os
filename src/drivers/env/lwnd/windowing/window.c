#include "window.h"
#include "dev/video/framebuffer.h"
#include "drivers/env/lwnd/display/screen.h"
#include "libk/flow/error.h"
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
    ret->this_fb = NULL;

    /* Link a windows initial rect */
    create_and_link_lwndrect(&ret->rects, ret->rect_cache, 0, 0, width, height);

    return ret;
}

void destroy_window(lwnd_window_t* window)
{
    destroy_zone_allocator(window->rect_cache, false);

    if (window->this_fb) {

        if (window->this_fb->kernel_addr)
            __kmem_kernel_dealloc(window->this_fb->kernel_addr, window->this_fb->size);

        kfree(window->this_fb);
    }

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
 *
 * FIXME: Check for negative overflow
 */
int lwnd_window_move(struct lwnd_screen* screen, lwnd_window_t* wnd, u32 newx, u32 newy)
{
    if (!wnd)
        return -KERR_INVAL;

    if (newx >= screen->px_width)
        return -KERR_SIZE_MISMATCH;

    if (newy >= screen->px_height)
        return -KERR_SIZE_MISMATCH;

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
    u32 draw_width;
    u32 draw_height;
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

    /* Calculate actual draw width and height */
    draw_height = rect->h - ((abs_rect_y + rect->h) > screen_info->height ? ((abs_rect_y + rect->h) - screen_info->height) : 0);
    draw_width = rect->w - ((abs_rect_x + rect->w) > screen_info->width ? ((abs_rect_x + rect->w) - screen_info->width) : 0);
    // KLOG_DBG("Trying to draw: x:%d y:%d (%dx%d)\n", rect->x, rect->y, rect->w, rect->h);

    /*
     * Our manual slow bitblt xD
     */
    for (uint32_t i = 0; i < draw_height; i++) {
        for (uint32_t j = 0; j < draw_width; j++)
            *(uint32_t volatile*)(screen_offset + j * screen_bytes_per_pixel) = *(uint32_t*)(rect_offset + j * wnd_bytes_per_pixel);

        screen_offset += screen_info->pitch;
        rect_offset += wnd_info->pitch;
    }
}

int lwnd_window_draw(lwnd_window_t* wnd, struct lwnd_screen* screen)
{
    for (lwnd_wndrect_t* r = wnd->rects; r; r = r->next_part) {
        if (r->rect_changed)
            __lwnd_redraw_rect(wnd, screen, r);
    }

    return 0;
}

/*!
 * @brief: Requests a framebuffer for this window to draw in
 */
int lwnd_window_request_fb(lwnd_window_t* wnd, lwnd_screen_t* screen)
{
    int error;
    u32 bytes_pp;
    fb_info_t* info;
    paddr_t fb_phys;

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

    /* First, allocate a contiguous range in the kernel */
    error = __kmem_kernel_alloc_range((void**)&info->kernel_addr, info->size, NULL, KMEM_FLAG_WRITABLE | KMEM_FLAG_KERNEL);

    if (error)
        goto error_and_exit;

    /* Clear out the new buffer */
    memset((void*)info->kernel_addr, 0, info->size);

    if (wnd->proc) {
        /* Grab the physical address of the framebuffer */
        fb_phys = kmem_to_phys(NULL, info->kernel_addr);

        if (!fb_phys)
            goto dealloc_and_exit;

        /* Allocate inside the userprocess */
        error = kmem_user_alloc((void**)&info->addr, wnd->proc, fb_phys, info->size, NULL, KMEM_FLAG_WRITABLE);

        if (error)
            goto dealloc_and_exit;
    } else
        /* System-owned, address will always be in the kernel (I hope) */
        info->addr = info->kernel_addr;

    wnd->this_fb = info;
    return 0;

dealloc_and_exit:
    __kmem_kernel_dealloc(info->kernel_addr, info->size);
error_and_exit:
    kfree(info);
    return error;
}
