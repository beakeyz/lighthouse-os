#include "screen.h"
#include "drivers/env/lwnd/windowing/window.h"
#include "drivers/env/lwnd/windowing/workspace.h"
#include "libk/stddef.h"
#include "mem/heap.h"

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

    kfree(screen);
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
