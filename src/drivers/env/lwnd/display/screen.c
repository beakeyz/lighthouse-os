#include "screen.h"
#include "drivers/env/lwnd/windowing/window.h"
#include "drivers/env/lwnd/windowing/workspace.h"
#include "libk/data/bitmap.h"
#include "libk/flow/error.h"
#include "libk/stddef.h"
#include "mem/heap.h"

/*!
 * @brief: Initialize the screen sectors for a screen
 *
 * Assumes that the width and height of the screen have already been set
 */
static int __lwnd_screen_init_sectors(lwnd_screen_t* screen, u32 sz_sector_log2)
{
    if (sz_sector_log2 > LWND_SCREEN_MAX_SECTOR_LOG2_SZ)
        return -KERR_INVAL;

    screen->sz_screen_sector_log2 = sz_sector_log2;
    screen->screen_sector_bitmap = create_bitmap_ex(((screen->px_width >> sz_sector_log2) + 1) * ((screen->px_height >> sz_sector_log2) + 1), 0xff);
    return 0;
}

/*!
 * @brief: Initialize a single screen object
 *
 * Right now this initialization is only based on a single fb_info object,
 * but there might be other things we need to know about the screen xD
 */
lwnd_screen_t* create_lwnd_screen(video_device_t* vdev, fb_info_t* fb)
{
    int error;
    lwnd_screen_t* ret;

    if (!vdev || !fb)
        return nullptr;

    ret = kmalloc(sizeof(*ret));

    if (!ret)
        return nullptr;

    memset(ret, 0, sizeof(*ret));

    ret->vdev = vdev;
    ret->fbinfo = fb;
    ret->px_width = fb->width;
    ret->px_height = fb->height;
    ret->mode_setting_id = 0;
    ret->c_workspace = 0;
    ret->n_workspace = 4;

    if (__lwnd_screen_init_sectors(ret, 3))
        goto free_and_exit;

    ret->background_window = create_window(nullptr, "__background__", 0, 0, ret->px_width, ret->px_height);

    if (!ret->background_window)
        goto free_and_exit;

    /* Request a framebuffer for the background window */
    error = lwnd_window_request_fb(ret->background_window, ret);

    if (error)
        goto destroy_and_exit;

    /* Fill the buffer with the default background image (or color lmao) */
    if (ret->background_window->this_fb && ret->background_window->this_fb->kernel_addr)
        memset((void*)ret->background_window->this_fb->kernel_addr, 0x1f, ret->background_window->this_fb->size);

    /* Initialize the workspaces last */
    init_lwnd_workspaces(ret->workspaces, &ret->n_workspace, ret);

    return ret;

destroy_and_exit:
    destroy_window(ret->background_window);
free_and_exit:
    kfree(ret);
    return nullptr;
}

void destroy_lwnd_screen(lwnd_screen_t* screen)
{
    for (u32 i = 0; i < screen->n_workspace; i++)
        destroy_lwnd_workspace(&screen->workspaces[i]);

    destroy_bitmap(screen->screen_sector_bitmap);
    kfree(screen);
}

/*!
 * @brief: Updates the sectors that @rect fits in
 */
int lwnd_screen_update_rect(lwnd_screen_t* screen, u32 x, u32 y, u32 w, u32 h)
{
    if (!screen || !w || !h)
        return -KERR_INVAL;

    /* Precompute some scalars */
    const u32 screen_width_sectors = screen->px_width >> screen->sz_screen_sector_log2;
    const u32 sector_width = w >> screen->sz_screen_sector_log2;
    const u32 sector_height = h >> screen->sz_screen_sector_log2;

    /* Compute the starting sector index */
    u32 sector_idx = (y >> screen->sz_screen_sector_log2) * screen_width_sectors + (x >> screen->sz_screen_sector_log2);

    /* Loop over the entire height of the rectangle */
    for (u32 i = 0; i < sector_height; i++) {
        /* Mark the sector */
        bitmap_mark_range(screen->screen_sector_bitmap, sector_idx, sector_width);

        /* Add */
        sector_idx += screen_width_sectors;
    }

    return 0;
}

/*!
 * @brief: Checks if a single sector needs to get updated
 */
bool lwnd_screen_sector_need_update(lwnd_screen_t* screen, u32 sector)
{
    return bitmap_isset(screen->screen_sector_bitmap, sector);
}

int lwnd_screen_update_sector(lwnd_screen_t* screen, u32 sector)
{
    if (!screen)
        return -KERR_INVAL;

    /* Unmark the entry in the bitmap */
    bitmap_unmark(screen->screen_sector_bitmap, sector);

    return 0;
}

lwnd_workspace_t* lwnd_screen_get_c_workspace(lwnd_screen_t* screen)
{
    return &screen->workspaces[screen->c_workspace];
}

lwnd_workspace_t* lwnd_screen_get_workspace(lwnd_screen_t* screen, u32 id)
{
    if (id >= screen->n_workspace)
        return nullptr;

    return &screen->workspaces[id];
}
