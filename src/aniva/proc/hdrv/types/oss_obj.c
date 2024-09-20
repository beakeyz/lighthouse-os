
#include "libk/flow/error.h"
#include "lightos/handle_def.h"
#include "oss/obj.h"
#include "proc/hdrv/driver.h"

static int ossobj_khdrv_close(khandle_driver_t* driver, khandle_t* handle)
{
    oss_obj_t* obj;

    obj = handle->reference.oss_obj;

    if (!obj)
        return -KERR_INVAL;

    return oss_obj_close(obj);
}

khandle_driver_t ossobj_khdrv = {
    .name = "oss objects",
    .handle_type = HNDL_TYPE_OSS_OBJ,
    .function_flags = KHDRIVER_FUNC_OPEN | KHDRIVER_FUNC_OPEN_REL | KHDRIVER_FUNC_CLOSE,
    .f_open = khandle_driver_generic_oss_open,
    .f_open_relative = khandle_driver_generic_oss_open_from,
    .f_close = ossobj_khdrv_close,
};
EXPORT_KHANDLE_DRIVER(ossobj_khdrv);
