#include "lightos/handle_def.h"
#include "proc/hdrv/driver.h"

/*
 * TODO:
 */

khandle_driver_t lib_khandle_drv = {
    .name = "SharedLib",
    .handle_type = HNDL_TYPE_SHARED_LIB,
};
EXPORT_KHANDLE_DRIVER(lib_khandle_drv);
