#include "sys_open.h"
#include "dev/core.h"
#include "libk/flow/error.h"
#include "lightos/driver/loader.h"
#include "lightos/handle_def.h"
#include "lightos/syscall.h"
#include "logging/log.h"
#include "mem/kmem_manager.h"
#include "oss/node.h"
#include "proc/core.h"
#include "proc/handle.h"
#include "proc/hdrv/driver.h"
#include "proc/proc.h"
#include "sched/scheduler.h"
#include "system/profile/profile.h"
#include "system/sysvar/map.h"
#include <proc/env.h>

/*
 * Generic open syscall
 *
 * TODO: Refactor
 * We can make this much cleaner. Every handle type can have it's own sys_open implementation, which
 * we just have to look up. This way we don't need this giant ugly switch statement
 */
HANDLE sys_open(const char* __user path, HANDLE_TYPE type, uint32_t flags, uint32_t mode)
{
    HANDLE ret;
    khandle_driver_t* khndl_driver;
    khandle_t handle = { 0 };
    kerror_t error;
    proc_t* c_proc;
    KLOG_DBG("AA: %s\n", path);

    c_proc = get_current_proc();

    if (!c_proc || (path && (kmem_validate_ptr(c_proc, (uintptr_t)path, 1))))
        return HNDL_INVAL;

    khndl_driver = nullptr;

    /* Find the driver for this handle type */
    if (khandle_driver_find(type, &khndl_driver) || !khndl_driver)
        return HNDL_INVAL;

    /* Just mark it as 'not found' if the driver fails to open */
    if (khandle_driver_open(khndl_driver, path, flags, mode, &handle))
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

    KLOG_DBG("CCCCCCC %d %s\n", ret, path);
    return ret;
}

HANDLE sys_open_rel(HANDLE rel_handle, const char* __user path, HANDLE_TYPE type, uint32_t flags, uint32_t mode)
{
    HANDLE ret;
    proc_t* c_proc;
    khandle_t* rel_khandle;
    khandle_t new_handle = { 0 };
    khandle_driver_t* khdriver;

    /* If there is no driver for this type, we can't really do much lmao */
    // if (khandle_driver_find(type, &khdriver))
    // return HNDL_INVAL;
    KLOG_DBG("AAAAA: %s\n", path);

    // TEMP: Assert that we can find a khandle driver here until we have implemented all base
    // khandle drivers
    ASSERT_MSG(khandle_driver_find(type, &khdriver) == 0 && khdriver, "sys_open: Failed to find khandle driver");

    /* grab this proc */
    c_proc = get_current_proc();

    if (kmem_validate_ptr(c_proc, (u64)path, sizeof(const char*)))
        return HNDL_INVAL;

    /* Find the handle which has our relative node */
    rel_khandle = find_khandle(&c_proc->m_handle_map, rel_handle);

    if (!rel_khandle || !rel_khandle->reference.kobj)
        return HNDL_INVAL;

    /* Open the driver */
    if (khandle_driver_open_relative(khdriver, rel_khandle, path, flags, mode, &new_handle))
        return HNDL_NOT_FOUND;

    ret = 0;

    /* Bind the thing */
    if (bind_khandle(&c_proc->m_handle_map, &new_handle, (u32*)&ret))
        return HNDL_INVAL;

    KLOG_DBG("RELLLLLLLL %d %s\n", ret, path);
    return ret;
}

vaddr_t sys_get_funcaddr(HANDLE lib_handle, const char __user* path)
{
    proc_t* c_proc;
    vaddr_t out;

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

uintptr_t sys_close(HANDLE handle)
{
    khandle_t* c_handle;
    kerror_t error;
    proc_t* current_process;

    current_process = get_current_proc();

    c_handle = find_khandle(&current_process->m_handle_map, handle);

    if (!c_handle)
        return SYS_INV;

    /* Destroying the khandle */
    error = unbind_khandle(&current_process->m_handle_map, c_handle);

    if (error)
        return SYS_ERR;

    return SYS_OK;
}

/*!
 * @brief: Syscall entry for the creation of profile variables
 *
 * NOTE: this syscall kinda assumes that the caller has permission to create variables on this profile if
 * it managed to get a handle with write access. This means that permissions need to be managed correctly
 * by sys_open in that regard. See the TODO comment inside sys_open
 */
bool sys_create_pvar(HANDLE handle, const char* __user name, enum SYSVAR_TYPE type, uint32_t flags, void* __user value)
{
    proc_t* proc;
    khandle_t* khandle;
    uint16_t target_prv_lvl;
    oss_node_t* target_node;

    proc = get_current_proc();

    if ((kmem_validate_ptr(proc, (uintptr_t)name, 1)))
        return false;

    if ((kmem_validate_ptr(proc, (uintptr_t)value, 1)))
        return false;

    khandle = find_khandle(&proc->m_handle_map, handle);

    /* Invalid handle =/ */
    if (!khandle)
        return false;

    /* Can't write to this handle =/ */
    if ((khandle->flags & HNDL_FLAG_W) != HNDL_FLAG_W)
        return false;

    switch (khandle->type) {
    case HNDL_TYPE_PROFILE:
        target_node = khandle->reference.profile->node;
        target_prv_lvl = khandle->reference.profile->priv_level;
        break;
    case HNDL_TYPE_PROC_ENV:
        target_node = khandle->reference.penv->node;
        target_prv_lvl = khandle->reference.penv->priv_level;
        break;
    default:
        target_node = nullptr;
        break;
    }

    if (!target_node)
        return false;

    if (KERR_ERR(sysvar_attach(target_node, name, target_prv_lvl, type, flags, (uintptr_t)value)))
        return false;

    return true;
}
