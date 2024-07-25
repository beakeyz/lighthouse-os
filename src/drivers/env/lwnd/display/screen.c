#include "screen.h"
#include "drivers/env/lwnd/windowing/workspace.h"
#include "libk/stddef.h"
#include "mem/heap.h"
#include <string.h>

/*!
 * @brief: Initialize a single screen object
 *
 * Right now this initialization is only based on a single fb_info object,
 * but there might be other things we need to know about the screen xD
 */
lwnd_screen_t* create_lwnd_screen(video_device_t* vdev, fb_info_t* fb)
{
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

    /* Initialize the workspaces last */
    init_lwnd_workspaces(ret->workspaces, &ret->n_workspace, ret);

    return ret;
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
