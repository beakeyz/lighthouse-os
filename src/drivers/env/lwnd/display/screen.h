#ifndef __LWND_DISPLAY_SCREEN__
#define __LWND_DISPLAY_SCREEN__

#include "dev/video/device.h"
#include "dev/video/framebuffer.h"
#include "drivers/env/lwnd/windowing/window.h"
#include "drivers/env/lwnd/windowing/workspace.h"

/* Allow sectors to be at max 512 pixels wide and high */
#define LWND_SCREEN_MAX_SECTOR_LOG2_SZ 9

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
    /* How big are the screen sectors */
    u32 sz_screen_sector_log2;

    /* Bitmap to store the update-state of every screen-sector */
    bitmap_t* screen_sector_bitmap;

    /* The window used for displaying the background. Image can be changed
     * with a driver control message
     */
    lwnd_window_t* background_window;

    lwnd_workspace_t workspaces[MAX_WORKSPACES];
} lwnd_screen_t;

lwnd_screen_t* create_lwnd_screen(video_device_t* vdev, fb_info_t* fb);
void destroy_lwnd_screen(lwnd_screen_t* screen);

/*
int lwnd_screen_update_rect(lwnd_screen_t* screen, u32 x, u32 y, u32 w, u32 h);
bool lwnd_screen_sector_need_update(lwnd_screen_t* screen, u32 sector);
int lwnd_screen_update_sector(lwnd_screen_t* screen, u32 sector);
*/

lwnd_workspace_t* lwnd_screen_get_c_workspace(lwnd_screen_t* screen);
lwnd_workspace_t* lwnd_screen_get_workspace(lwnd_screen_t* screen, u32 id);

#endif // !__LWND_DISPLAY_SCREEN__
