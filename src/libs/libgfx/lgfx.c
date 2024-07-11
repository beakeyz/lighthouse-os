#include "include/lgfx.h"
#include "libgfx/include/driver.h"
#include "lightos/driver/drv.h"
#include "lightos/handle.h"
#include "lightos/var/var.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

BOOL get_lwnd_drv_path(char* buffer, size_t bufsize)
{
    return sysvar_read_from_profile("User", LWND_DRV_PATH_VAR, NULL, bufsize, (void*)buffer);
}

BOOL request_lwindow(lwindow_t* wnd, DWORD width, DWORD height, DWORD flags)
{
    BOOL res;
    char lwnd_drv_path[128] = { 0 };

    if (!wnd || !width || !height)
        return FALSE;

    memset(wnd, 0, sizeof(*wnd));

    res = get_lwnd_drv_path(lwnd_drv_path, sizeof(lwnd_drv_path));

    if (!res || lwnd_drv_path[0] == NULL)
        return FALSE;

    /* Prepare our window object */
    wnd->current_height = height;
    wnd->current_width = width;
    wnd->wnd_flags = flags;
    wnd->keyevent_buffer_capacity = LWND_DEFAULT_EVENTBUFFER_CAPACITY;
    wnd->keyevent_buffer = malloc(wnd->keyevent_buffer_capacity * sizeof(lkey_event_t));

    /* Open lwnd */
    res = open_driver(lwnd_drv_path, NULL, NULL, &wnd->lwnd_handle);

    if (!res)
        return FALSE;

    /* Send it a message that we want to make a window */
    return driver_send_msg(wnd->lwnd_handle, LWND_DCC_CREATE, wnd, sizeof(*wnd));
}

BOOL close_lwindow(lwindow_t* wnd)
{
    BOOL res;

    if (!wnd)
        return FALSE;

    res = driver_send_msg(wnd->lwnd_handle, LWND_DCC_CLOSE, wnd, sizeof(*wnd));

    if (!res)
        return FALSE;

    free(wnd->keyevent_buffer);

    wnd->keyevent_buffer = NULL;
    wnd->keyevent_buffer_capacity = NULL;
    wnd->keyevent_buffer_read_idx = NULL;
    wnd->keyevent_buffer_write_idx = NULL;

    /* Make sure the handle to the driver gets closed here */
    return close_handle(wnd->lwnd_handle);
}
