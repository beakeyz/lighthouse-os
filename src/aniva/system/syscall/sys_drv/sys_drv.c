#include "sys_drv.h"
#include "dev/core.h"
#include "dev/device.h"
#include "fs/file.h"
#include "libk/flow/error.h"
#include "lightos/handle_def.h"
#include "lightos/syscall.h"
#include "mem/kmem_manager.h"
#include "oss/obj.h"
#include "proc/handle.h"
#include "proc/proc.h"
#include "sched/scheduler.h"
#include "system/sysvar/var.h"
#include <dev/driver.h>
#include <proc/env.h>

#include <lightos/driver/ctl.h>

uintptr_t
sys_send_message(HANDLE handle, driver_control_code_t code, void* buffer, size_t size)
{
    kerror_t error;
    khandle_t* c_hndl;
    proc_t* c_proc;

    c_proc = get_current_proc();

    /* Check the buffer(s) if they are given */
    if (buffer && kmem_validate_ptr(c_proc, (vaddr_t)buffer, size))
        return SYS_INV;

    /* Find the handle */
    c_hndl = find_khandle(&c_proc->m_handle_map, handle);

    if (!c_hndl)
        return SYS_INV;

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
        return SYS_INV;
    }

    if (error)
        return SYS_ERR;

    return SYS_OK;
}

uintptr_t sys_send_ctl(HANDLE handle, enum DEVICE_CTLC code, u64 offset, void* buffer, size_t bsize)
{
    int error;
    khandle_t* c_hndl;
    proc_t* c_proc;

    c_proc = get_current_proc();

    /* Check the buffer(s) if they are given */
    if (buffer && kmem_validate_ptr(c_proc, (vaddr_t)buffer, bsize))
        return SYS_INV;

    /* Find the handle */
    c_hndl = find_khandle(&c_proc->m_handle_map, handle);

    if (!c_hndl)
        return SYS_INV;

    switch (c_hndl->type) {
    case HNDL_TYPE_DEVICE:
        if (!c_hndl->reference.device)
            return SYS_ERR;

        if (!oss_obj_can_proc_access(c_hndl->reference.device->obj, c_proc))
            return SYS_NOPERM;

        error = device_send_ctl_ex(c_hndl->reference.device, code, offset, buffer, bsize);
        break;
    default:
        error = -KERR_NOT_FOUND;
        break;
    }

    if (error)
        return SYS_ERR;

    return SYS_OK;
}

uintptr_t
sys_get_handle_type(HANDLE handle)
{
    khandle_t* c_handle;
    proc_t* c_proc;

    c_proc = get_current_proc();
    c_handle = find_khandle(&c_proc->m_handle_map, handle);

    if (!c_handle)
        return HNDL_TYPE_NONE;

    return c_handle->type;
}

bool sys_get_pvar_type(HANDLE pvar_handle, enum SYSVAR_TYPE __user* type_buffer)
{
    sysvar_t* var;
    khandle_t* handle;
    proc_t* current_proc;

    current_proc = get_current_proc();

    /* Check the buffer address */
    if (kmem_validate_ptr(current_proc, (uint64_t)type_buffer, sizeof(type_buffer)))
        return false;

    /* Find the khandle */
    handle = find_khandle(&current_proc->m_handle_map, pvar_handle);

    if (handle->type != HNDL_TYPE_SYSVAR)
        return false;

    /* Extract the profile variable */
    var = handle->reference.pvar;

    if (!var)
        return false;

    /*
     * Set the userspace buffer
     * TODO: create a kmem routine that handles memory opperations to and from
     * userspace
     */
    *type_buffer = var->type;
    return true;
}
