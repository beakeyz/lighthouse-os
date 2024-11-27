#include "proc/proc.h"
#include "libk/flow/error.h"
#include "lightos/handle_def.h"
#include "proc/core.h"
#include "proc/handle.h"
#include "proc/hdrv/driver.h"

static int __proc_open(struct khandle_driver* driver, const char* path, u32 flags, enum HNDL_MODE mode, khandle_t* pHandle)
{
    proc_t* target;

    target = find_proc(path);

    if (!target)
        return -KERR_NOT_FOUND;

    /* Initialize the handle */
    init_khandle_ex(pHandle, driver->handle_type, flags, target);

    return 0;
}

static int __proc_open_rel(struct khandle_driver* driver, khandle_t* rel_handle, const char* path, u32 flags, enum HNDL_MODE mode, khandle_t* phandle)
{
    return 0;
}

static khandle_driver_t __proc_driver = {
    .name = "processes",
    .handle_type = HNDL_TYPE_PROC,
    .function_flags = KHDRIVER_FUNC_OPEN,
    .f_open = __proc_open,
};
EXPORT_KHANDLE_DRIVER(__proc_driver);
