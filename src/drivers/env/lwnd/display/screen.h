#ifndef __LWND_DISPLAY_SCREEN__
#define __LWND_DISPLAY_SCREEN__

#include "dev/video/device.h"
#include "dev/video/framebuffer.h"
#include "drivers/env/lwnd/windowing/window.h"
#include "drivers/env/lwnd/windowing/workspace.h"

/*
 * lwnd screen representation
 *
 * Highest level struct inside lwnd for window managing.
 * (screen -> workspace -> stack -> window)
 *
 * The screen is responsible for
 */

typedef struct lwnd_screen {
    fb_info_t* fbinfo;

    /* The video device responsible for this screen */
    video_device_t* vdev;

    /* Resolution */
    u32 px_width;
    u32 px_height;
    /* TODO: Mode settings */
    u32 mode_setting_id;
    u32 c_workspace;
    u32 n_workspace;

    /* The window used for displaying the background. Image can be changed
     * with a driver control message
     */
    lwnd_window_t* background_window;

    lwnd_workspace_t workspaces[MAX_WORKSPACES];
} lwnd_screen_t;

lwnd_screen_t* create_lwnd_screen(video_device_t* vdev, fb_info_t* fb);
void destroy_lwnd_screen(lwnd_screen_t* screen);

lwnd_workspace_t* lwnd_screen_get_c_workspace(lwnd_screen_t* screen);
lwnd_workspace_t* lwnd_screen_get_workspace(lwnd_screen_t* screen, u32 id);

#endif // !__LWND_DISPLAY_SCREEN__
