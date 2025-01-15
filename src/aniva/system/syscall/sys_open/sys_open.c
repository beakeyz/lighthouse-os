#include "dev/core.h"
#include "libk/flow/error.h"
#include "lightos/api/dynldr.h"
#include "lightos/api/handle.h"
#include "lightos/syscall.h"
#include "mem/kmem.h"
#include "proc/handle.h"
#include "proc/hdrv/driver.h"
#include "proc/proc.h"
#include "sched/scheduler.h"
#include <lightos/types.h>
#include <proc/env.h>

/*
 * Generic open syscall
 *
 * TODO: Refactor
 * We can make this much cleaner. Every handle type can have it's own sys_open implementation, which
 * we just have to look up. This way we don't need this giant ugly switch statement
 */
HANDLE sys_open(const char __user* path, handle_flags_t flags, enum HNDL_MODE mode, void __user* buffer, size_t bsize)
{
    HANDLE ret;
    khandle_driver_t* khndl_driver;
    khandle_t* rel_khndl;
    khandle_t handle = { 0 };
    kerror_t error;
    proc_t* c_proc;

    c_proc = get_current_proc();

    if (!c_proc || !path || (kmem_validate_ptr(c_proc, (uintptr_t)path, 1)))
        return HNDL_INVAL;

    if (buffer && kmem_validate_ptr(c_proc, (vaddr_t)buffer, bsize))
        return HNDL_INVAL;

    khndl_driver = nullptr;

    /* Find the driver for this handle type */
    if (khandle_driver_find(flags.s_type, &khndl_driver) || !khndl_driver)
        return HNDL_INVAL;

    if (HNDL_IS_VALID(flags.s_rel_hndl)) {
        rel_khndl = find_khandle(&c_proc->m_handle_map, flags.s_rel_hndl);

        /* Invalid relative handle anyway =( */
        if (!rel_khndl)
            return HNDL_INVAL;

        if (khandle_driver_open_relative(khndl_driver, rel_khndl, path, flags.s_flags, mode, &handle))
            return HNDL_NOT_FOUND;
    } else
        /* Just mark it as 'not found' if the driver fails to open */
        if (khandle_driver_open(khndl_driver, path, flags.s_flags, mode, &handle))
            return HNDL_NOT_FOUND;

    /*
     * TODO: check for permissions and open with the appropriate flags
     *
     * We want to do this by checking the profile that this process is in and checking if processes
     * in that profile have access to this resource
     */

    /* Check if the handle was really initialized */
    if (!handle.reference.kobj)
        return HNDL_NOT_FOUND;

    /* Copy the handle into the map */
    error = bind_khandle(&c_proc->m_handle_map, &handle, (uint32_t*)&ret);

    if (error)
        ret = HNDL_NOT_FOUND;

    return ret;
}

void* sys_get_function(HANDLE lib_handle, const char __user* path)
{
    proc_t* c_proc;
    void* out;

    c_proc = get_current_proc();

    if (kmem_validate_ptr(c_proc, (vaddr_t)path, 1))
        return NULL;

    /* NOTE: Dangerous */
    if (!strlen(path))
        return NULL;

    if (driver_send_msg_a(DYN_LDR_URL, DYN_LDR_GET_FUNC_ADDR, (void*)path, sizeof(path), &out, sizeof(out)))
        return NULL;

    return out;
}

error_t sys_close(HANDLE handle)
{
    kerror_t error;
    proc_t* current_process;

    current_process = get_current_proc();

    /* Destroying the khandle */
    error = unbind_khandle(&current_process->m_handle_map, handle);

    if (error)
        return EINVAL;

    return 0;
}
