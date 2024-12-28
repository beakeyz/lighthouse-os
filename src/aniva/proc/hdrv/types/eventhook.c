#include "kevent/event.h"
#include "kevent/hook.h"
#include "libk/flow/error.h"
#include "lightos/dev/shared.h"
#include "lightos/handle_def.h"
#include "oss/path.h"
#include "proc/handle.h"
#include "proc/hdrv/driver.h"

static int keventhook_hdrv_open(khandle_driver_t* driver, const char* path, u32 flags, enum HNDL_MODE mode, khandle_t* bHandle)
{
    error_t error;
    kevent_hook_t* hook;
    oss_path_t oss_path;

    if (!driver || !path)
        return -EINVAL;

    error = oss_parse_path_ex(path, &oss_path, '.');

    if (error)
        return error;

    if (oss_path.n_subpath < 2 || !oss_path.subpath_vec)
        return -EINVAL;

    if (mode == HNDL_MODE_CREATE || mode == HNDL_MODE_CREATE_NEW) {
        error = kevent_add_poll_hook(oss_path.subpath_vec[0], oss_path.subpath_vec[1], nullptr, NULL);

        if (error)
            return error;
    }

    hook = kevent_get_hook(kevent_get(oss_path.subpath_vec[0]), oss_path.subpath_vec[1]);

    /* Failed to create the hook? */
    if (!hook)
        return -ENOENT;

    init_khandle_ex(bHandle, driver->handle_type, flags, hook);

    return 0;
}

static error_t keventhook_hdrv_ctl(khandle_driver_t* driver, khandle_t* handle, enum DEVICE_CTLC code, u64 offset, void* buffer, size_t bsize)
{
    error_t error = -ENOTSUP;
    kevent_hook_t* hook;
    kevent_hook_poll_block_t* block = nullptr;

    if (!driver || !handle)
        return -EINVAL;

    /* Grab the event */
    hook = handle->reference.hook;

    switch (code) {
    case DEVICE_CTLC_KEVENT_POLL:
        error = kevent_hook_poll(hook, &block);
        break;
    case DEVICE_CTLC_KEVENT_AWAIT_FIRE:
        error = kevent_hook_poll_await_fire(hook, 0, &block);
        break;
    default:
        break;
    }

    if (error)
        return error;

    /* No event found =( */
    if (!block)
        return -ENOENT;

    kernel_panic("TODO: translate the kernel event context to something userspace can use");

    return 0;
}

khandle_driver_t keventhook_handle_driver = {
    .name = "keventhooks",
    .handle_type = HNDL_TYPE_EVENTHOOK,
    .function_flags = KHDRIVER_FUNC_OPEN | KHDRIVER_FUNC_CTL,
    .f_open = keventhook_hdrv_open,
    .f_ctl = keventhook_hdrv_ctl,
};
EXPORT_KHANDLE_DRIVER(keventhook_handle_driver);
