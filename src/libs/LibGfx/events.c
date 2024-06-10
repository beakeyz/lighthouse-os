#include "include/events.h"
#include "LibGfx/include/driver.h"
#include "lightos/driver/drv.h"

BOOL cache_keyevent(lwindow_t* wnd)
{
    return driver_send_msg(wnd->lwnd_handle, LWND_DCC_GETKEY, wnd, sizeof(*wnd));
}

BOOL get_key_event(lwindow_t* wnd, lkey_event_t* keyevent)
{
    if (!wnd)
        return FALSE;

    if (keyevent) {
        if (wnd->keyevent_buffer_read_idx == wnd->keyevent_buffer_write_idx)
            return FALSE;

        *keyevent = wnd->keyevent_buffer[wnd->keyevent_buffer_read_idx++];
        wnd->keyevent_buffer_read_idx %= wnd->keyevent_buffer_capacity;
        return TRUE;
    }

    return cache_keyevent(wnd);
}
