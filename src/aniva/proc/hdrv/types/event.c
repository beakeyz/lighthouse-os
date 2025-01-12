#include "libk/flow/error.h"
#include "lightos/api/handle.h"
#include "proc/handle.h"
#include "proc/hdrv/driver.h"
#include <kevent/event.h>

/*!
 * @brief: Handles user requests to open a kernel event
 *
 * This function can serve a couple of purposes:
 * 1) Create new kernel events
 * 2) Open existing kernel events
 * 3) Open new event hooks on events (path format: "<kevent>.<keventhook>")
 */
static int kevent_hdrv_open(khandle_driver_t* driver, const char* path, u32 flags, enum HNDL_MODE mode, khandle_t* bHandle)
{
    struct kevent* event;

    if (!path)
        return 0;

    event = kevent_get(path);

    switch (mode) {
    case HNDL_MODE_CREATE_NEW:
        /* Check if the event already exists */
        if (event)
            return -KERR_DUPLICATE;

        /* Fallthrough, since creation is the same for these two modes3 */
    case HNDL_MODE_CREATE:
        if (event)
            break;

        /* In this case, flags are used as kevent flags */
        if (add_kevent(path, KE_CUSTOM_EVENT, flags, 32))
            return -KERR_NULL;

        /* Get the event again, so we can initialize the handle */
        event = kevent_get(path);
        break;
    default:
        break;
    }

    if (!event)
        return -KERR_NOT_FOUND;

    /* Initialize this handle */
    init_khandle_ex(bHandle, driver->handle_type, flags, event);
    return 0;
}

static error_t kevent_hdrv_ctl(khandle_driver_t* driver, khandle_t* handle, enum DEVICE_CTLC code, u64 offset, void* buffer, size_t bsize)
{
    struct kevent* event;

    if (!driver || !handle)
        return -EINVAL;

    /* Grab the event */
    event = handle->reference.event;

    (void)event;

    switch (code) {
    default:
        return -ENOTSUP;
    }
    return 0;
}

khandle_driver_t kevent_handle_driver = {
    .name = "kevents",
    .handle_type = HNDL_TYPE_EVENT,
    .function_flags = KHDRIVER_FUNC_OPEN | KHDRIVER_FUNC_CTL,
    .f_open = kevent_hdrv_open,
    .f_ctl = kevent_hdrv_ctl,
};
EXPORT_KHANDLE_DRIVER(kevent_handle_driver);
