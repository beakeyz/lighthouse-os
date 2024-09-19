#include "libk/flow/error.h"
#include "lightos/handle_def.h"
#include "proc/handle.h"
#include "proc/hdrv/driver.h"
#include <kevent/event.h>

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

khandle_driver_t kevent_handle_driver = {
    .name = "kevents",
    .handle_type = HNDL_TYPE_EVENT,
    .function_flags = KHDRIVER_FUNC_OPEN,
    .f_open = kevent_hdrv_open,
};
EXPORT_KHANDLE_DRIVER(kevent_handle_driver);
