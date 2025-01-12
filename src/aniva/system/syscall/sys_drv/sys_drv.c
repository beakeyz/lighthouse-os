#include "dev/core.h"
#include "dev/device.h"
#include "fs/file.h"
#include "libk/flow/error.h"
#include <lightos/types.h>
#include "lightos/api/handle.h"
#include "lightos/syscall.h"
#include "mem/kmem.h"
#include "oss/obj.h"
#include "proc/handle.h"
#include "proc/proc.h"
#include "sched/scheduler.h"
#include <dev/driver.h>
#include <proc/env.h>

#include <lightos/api/driver.h>

error_t sys_send_msg(HANDLE handle, dcc_t code, u64 offset, void* buffer, size_t size)
{
    kerror_t error;
    khandle_t* c_hndl;
    proc_t* c_proc;

    c_proc = get_current_proc();

    /* Check the buffer(s) if they are given */
    if (buffer && kmem_validate_ptr(c_proc, (vaddr_t)buffer, size))
        return EINVAL;

    /* Find the handle */
    c_hndl = find_khandle(&c_proc->m_handle_map, handle);

    if (!c_hndl)
        return EINVAL;

    error = (0);

    switch (c_hndl->type) {
    case HNDL_TYPE_DRIVER:
        /* NOTE: this call does not lock the driver */
        error = driver_send_msg_ex(c_hndl->reference.driver, code, buffer, size, NULL, NULL);
        break;
    case HNDL_TYPE_DEVICE:
        error = device_send_ctl_ex(c_hndl->reference.device, code, NULL, buffer, size);
        break;
    case HNDL_TYPE_FILE: {
        file_t* file = c_hndl->reference.file;

        /* TODO: follow unix? */
        (void)file;
        error = -1;
        break;
    }
    default:
        return EINVAL;
    }

    if (error)
        return EINVAL;

    return 0;
}

error_t sys_send_ctl(HANDLE handle, enum DEVICE_CTLC code, u64 offset, void* buffer, size_t bsize)
{
    int error;
    khandle_t* c_hndl;
    proc_t* c_proc;

    c_proc = get_current_proc();

    /* Check the buffer(s) if they are given */
    if (buffer && kmem_validate_ptr(c_proc, (vaddr_t)buffer, bsize))
        return -EINVAL;

    /* Find the handle */
    c_hndl = find_khandle(&c_proc->m_handle_map, handle);

    if (!c_hndl)
        return -EINVAL;

    switch (c_hndl->type) {
    case HNDL_TYPE_DEVICE:
        if (!c_hndl->reference.device)
            return -EINVAL;

        if (!oss_obj_can_proc_access(c_hndl->reference.device->obj, c_proc))
            return EPERM;

        error = device_send_ctl_ex(c_hndl->reference.device, code, offset, buffer, bsize);
        break;
    default:
        error = -KERR_NOT_FOUND;
        break;
    }

    return error;
}
