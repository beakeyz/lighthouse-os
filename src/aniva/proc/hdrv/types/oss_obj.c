#include "lightos/api/handle.h"
#include "oss/core.h"
#include "oss/object.h"
#include "proc/handle.h"
#include "proc/hdrv/driver.h"

static int oss_object_khdrv_open(struct khandle_driver* driver, const char* path, u32 flags, enum HNDL_MODE mode, khandle_t* bHandle)
{
    error_t error;
    oss_object_t* object;

    switch (mode) {
    case HNDL_MODE_NORMAL:
        error = oss_open_object(path, &object);
        break;
    default:
        return -ENOIMPL;
    }

    if (error)
        return error;

    /* Initialize this handle */
    init_khandle_ex(bHandle, driver->handle_type, flags, object);

    return 0;
}

static int oss_object_khdrv_open_relative(struct khandle_driver* driver, khandle_t* rel_hndl, const char* path, u32 flags, enum HNDL_MODE mode, khandle_t* bHandle)
{
    error_t error;
    oss_object_t* object;
    oss_object_t* rel_object;

    if (!rel_hndl || rel_hndl->type != HNDL_TYPE_OBJECT)
        return -EINVAL;

    rel_object = rel_hndl->object;

    switch (mode) {
    case HNDL_MODE_NORMAL:
        error = oss_open_object_from(path, rel_object, &object);
        break;
    default:
        return -ENOIMPL;
    }

    if (error)
        return error;

    /* Initialize this handle */
    init_khandle_ex(bHandle, driver->handle_type, flags, object);

    return 0;
}

static int ossobj_khdrv_close(khandle_driver_t* driver, khandle_t* handle)
{
    return oss_object_close(handle->object);
}

int oss_object_khdrv_read(struct khandle_driver* driver, khandle_t* hndl, u64 offset, void* buffer, size_t bsize)
{
    return oss_object_read(hndl->object, offset, buffer, bsize);
}

int oss_object_khdrv_write(struct khandle_driver* driver, khandle_t* hndl, u64 offset, void* buffer, size_t bsize)
{
    return oss_object_write(hndl->object, offset, buffer, bsize);
}

khandle_driver_t ossobj_khdrv = {
    .name = "oss objects",
    .handle_type = HNDL_TYPE_OBJECT,
    .function_flags = KHDRIVER_FUNC_OPEN | KHDRIVER_FUNC_OPEN_REL | KHDRIVER_FUNC_CLOSE,
    .f_open = oss_object_khdrv_open,
    .f_open_relative = oss_object_khdrv_open_relative,
    .f_close = ossobj_khdrv_close,
    .f_read = oss_object_khdrv_read,
    .f_write = oss_object_khdrv_write,
};
EXPORT_KHANDLE_DRIVER(ossobj_khdrv);
