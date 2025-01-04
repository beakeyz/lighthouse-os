#include "lightos/handle_def.h"
#include "proc/hdrv/driver.h"

/*!
 * @brief: Basic virtual memory mapping open function
 */
int vmem_open(struct khandle_driver* driver, const char* path, u32 flags, enum HNDL_MODE mode, khandle_t* bHandle)
{

    switch (mode) {
        case HNDL_MODE_NORMAL:
        case HNDL_MODE_CREATE:
        case HNDL_MODE_CREATE_NEW:
            break;
        default:
            return -ENOTSUP;
    }

    return 0;
}

int vmem_open_rel(struct khandle_driver* driver, khandle_t* rel_hndl, const char* path, u32 flags, enum HNDL_MODE mode, khandle_t* bHandle)
{
    return 0;
}

khandle_driver_t vmem_driver = {
    .name = "vmem",
    .handle_type = HNDL_TYPE_VMEM,
    .function_flags = KHDRIVER_FUNC_OPEN | KHDRIVER_FUNC_OPEN_REL,
    .f_open = vmem_open,
    .f_open_relative = vmem_open_rel,
};
EXPORT_KHANDLE_DRIVER(vmem_driver);
