#include "lgfx.h"
#include "libgfx/shared.h"
#include "lightos/driver/drv.h"
#include "lightos/handle_def.h"
#include <lightos/lightos.h>
#include "lightos/sysvar/var.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static HANDLE lwnd_driver;

BOOL get_lwnd_drv_path(char* buffer, size_t bufsize)
{
    return sysvar_read_from_profile("User", LWND_DRV_PATH_VAR, NULL, (void*)buffer, bufsize);
}

BOOL request_lwindow(lwindow_t* p_wnd, const char* title, u32 width, u32 height, u32 flags)
{
    if (!p_wnd || !width || !height)
        return FALSE;

    memset(p_wnd, 0, sizeof(*p_wnd));

    /* Prepare our window object */
    p_wnd->title = strdup(title);
    p_wnd->current_height = height;
    p_wnd->current_width = width;
    p_wnd->wnd_flags = flags;
    p_wnd->keyevent_buffer_capacity = LWND_DEFAULT_EVENTBUFFER_CAPACITY;
    p_wnd->keyevent_buffer = malloc(p_wnd->keyevent_buffer_capacity * sizeof(lkey_event_t));
    p_wnd->lwnd_handle = lwnd_driver;

    /* Send it a message that we want to make a window */
    return driver_send_msg(p_wnd->lwnd_handle, LWND_DCC_CREATE, 0, p_wnd, sizeof(*p_wnd));
}

BOOL close_lwindow(lwindow_t* wnd)
{
    BOOL res;

    if (!wnd)
        return FALSE;

    res = driver_send_msg(wnd->lwnd_handle, LWND_DCC_CLOSE, 0, wnd, sizeof(*wnd));

    if (!res)
        return FALSE;

    free(wnd->keyevent_buffer);

    wnd->keyevent_buffer = NULL;
    wnd->keyevent_buffer_capacity = NULL;
    wnd->keyevent_buffer_read_idx = NULL;
    wnd->keyevent_buffer_write_idx = NULL;

    return TRUE;
}

/*!
 * @brief: Entry for shared library
 */
LIGHTENTRY int gfxlibentry(void)
{
    char drvname[128] = { 0 };

    /* Try to get the drivername from the system */
    if (!get_lwnd_drv_path(drvname, sizeof(drvname)))
        return -ENODEV;

    /* Open the driver */
    if (!open_driver(drvname, HNDL_FLAG_RW, HNDL_MODE_NORMAL, &lwnd_driver))
        return -ENODEV;

    printf("Initialized gfxlib!\n");
    return 0;
}
