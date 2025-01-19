#include "dev/core.h"
#include "libk/flow/error.h"
#include "lightos/api/handle.h"
#include "proc/handle.h"
#include "proc/hdrv/driver.h"
#include <dev/driver.h>
#include <lightos/api/dynldr.h>

/*
 * TODO:
 */

static int __lib_khdrv_open(struct khandle_driver* driver, const char* path, u32 flags, enum HNDL_MODE mode, khandle_t* bHandle)
{
    kerror_t error;
    dynamic_library_t* lib = nullptr;

    if (!path)
        return -KERR_INVAL;

    error = driver_send_msg_a(DYN_LDR_NAME, DYN_LDR_LOAD_LIB, (void*)path, sizeof(path), &lib, sizeof(dynamic_library_t*));

    if (error || !lib)
        return -KERR_NOT_FOUND;

    /* Initialize the handle */
    init_khandle_ex(bHandle, driver->handle_type, flags, lib);

    return 0;
}

khandle_driver_t lib_khandle_drv = {
    .name = "SharedLib",
    .handle_type = HNDL_TYPE_SHARED_LIB,
    .function_flags = KHDRIVER_FUNC_OPEN,
    .f_open = __lib_khdrv_open,
};
EXPORT_KHANDLE_DRIVER(lib_khandle_drv);
