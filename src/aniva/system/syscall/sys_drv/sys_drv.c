#include "dev/core.h"
#include "dev/device.h"
#include "libk/flow/error.h"
#include "lightos/api/handle.h"
#include "lightos/api/objects.h"
#include "lightos/syscall.h"
#include "mem/kmem.h"
#include "oss/object.h"
#include "proc/handle.h"
#include "proc/proc.h"
#include "sched/scheduler.h"
#include <dev/driver.h>
#include <lightos/types.h>

#include <lightos/api/driver.h>

/*!
 * @brief: Handles messages from processes to underlying types of object handles
 *
 *
 */
error_t sys_send_msg(HANDLE handle, dcc_t code, u64 offset, void* buffer, size_t size)
{
    kerror_t error;
    khandle_t* c_hndl;
    oss_object_t* object;
    proc_t* c_proc;

    c_proc = get_current_proc();

    /* Check the buffer(s) if they are given */
    if (buffer && kmem_validate_ptr(c_proc, (vaddr_t)buffer, size))
        return EINVAL;

    /* Find the handle */
    c_hndl = find_khandle(&c_proc->m_handle_map, handle);

    if (!c_hndl)
        return EINVAL;

    object = c_hndl->object;

    error = (0);

    switch (object->type) {
    case OT_DRIVER:
        /* NOTE: this call does not lock the driver */
        error = driver_send_msg_ex(oss_object_unwrap(object, OT_DRIVER), code, buffer, size, NULL, NULL);
        break;
    case OT_DEVICE:
        error = device_send_ctl_ex(oss_object_unwrap(object, OT_DEVICE), code, offset, buffer, size);
        break;
    default:
        return ENOIMPL;
    }

    if (error)
        return EINVAL;

    return 0;
}
