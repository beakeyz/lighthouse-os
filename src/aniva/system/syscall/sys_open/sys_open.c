#include "sys_open.h"
#include "dev/core.h"
#include "dev/device.h"
#include "dev/manifest.h"
#include "fs/dir.h"
#include "fs/file.h"
#include "kevent/event.h"
#include "libk/flow/error.h"
#include "lightos/driver/loader.h"
#include "lightos/handle_def.h"
#include "lightos/var/shared.h"
#include "lightos/syscall.h"
#include "logging/log.h"
#include "mem/kmem_manager.h"
#include "oss/core.h"
#include "oss/node.h"
#include "oss/obj.h"
#include "proc/core.h"
#include "proc/handle.h"
#include "proc/proc.h"
#include "sched/scheduler.h"
#include "system/profile/profile.h"
#include "system/sysvar/map.h"
#include <proc/env.h>

/*
 * Generic open syscall
 *
 * TODO: Refactor
 */
HANDLE sys_open(const char* __user path, HANDLE_TYPE type, uint32_t flags, uint32_t mode)
{
    HANDLE ret;
    khandle_t handle = { 0 };
    kerror_t error;
    proc_t* c_proc;

    KLOG_DBG("TRYINGO TO OPEN THING %s\n", path);

    c_proc = get_current_proc();

    if (!c_proc || (path && (kmem_validate_ptr(c_proc, (uintptr_t)path, 1))))
        return HNDL_INVAL;

    /*
     * TODO: check for permissions and open with the appropriate flags
     *
     * We want to do this by checking the profile that this process is in and checking if processes
     * in that profile have access to this resource
     */

    switch (type) {
    case HNDL_TYPE_FILE: {
        file_t* file;

        if (!path)
            return HNDL_INVAL;

        file = file_open(path);

        if (!file)
            return HNDL_NOT_FOUND;

        init_khandle(&handle, &type, file);
        break;
    }
    case HNDL_TYPE_DIR: {
        dir_t* dir;

        if (!path)
            return HNDL_INVAL;

        dir = dir_open(path);

        if (!dir)
            return HNDL_NOT_FOUND;

        init_khandle(&handle, &type, dir);
        break;
    }
    case HNDL_TYPE_DEVICE: {
        device_t* device;
        oss_obj_t* obj;

        if (oss_resolve_obj(path, &obj))
            return HNDL_NOT_FOUND;

        if (!obj)
            return HNDL_NOT_FOUND;

        /* Yikes */
        if (!oss_obj_can_proc_access(obj, c_proc)) {
            oss_obj_unref(obj);
            return HNDL_NO_PERM;
        }

        device = oss_obj_get_device(obj);

        if (!device) {
            oss_obj_unref(obj);
            return HNDL_INVAL;
        }

        init_khandle(&handle, &type, device);
        break;
    }
    case HNDL_TYPE_PROC: {
        /* Search through oss */
        proc_t* proc = find_proc(path);

        if (!proc)
            return HNDL_NOT_FOUND;

        /* Pretend we didn't find this one xD (If we're not admin) */
        if ((proc->m_flags & PROC_KERNEL) == PROC_KERNEL && c_proc->m_env->priv_level < PRIV_LVL_ADMIN)
            return HNDL_NOT_FOUND;

        /* We do allow handles to drivers */
        init_khandle(&handle, &type, proc);
        break;
    }
    case HNDL_TYPE_DRIVER: {
        drv_manifest_t* driver = get_driver(path);

        KLOG_DBG("Dk\n");

        if (!driver)
            return HNDL_NOT_FOUND;
        
        KLOG_DBG("hdhehelf\n");

        init_khandle(&handle, &type, driver);
        break;
    }
    case HNDL_TYPE_PROFILE: {
        user_profile_t* profile;

        profile = nullptr;

        switch (mode) {
        case HNDL_MODE_CURRENT_PROFILE:
            profile = c_proc->m_env->profile;
            break;
        case HNDL_MODE_SCAN_PROFILE:
            if (KERR_ERR(profile_find(path, &profile)))
                return HNDL_NOT_FOUND;
            break;
        }

        if (!profile)
            return HNDL_NOT_FOUND;

        init_khandle(&handle, &type, profile);
        break;
    }
    case HNDL_TYPE_PROC_ENV: {
        penv_t* env;
        oss_node_t* env_node;

        env = nullptr;

        switch (mode) {
        case HNDL_MODE_CURRENT_PROFILE:
            env = c_proc->m_env;
            break;
        case HNDL_MODE_SCAN_PROFILE:
            /*
             * TODO/FIXME: Should we extract this code into a routine inside
             * proc/env.c?
             */
            if (KERR_ERR(oss_resolve_node(path, &env_node)))
                return HNDL_NOT_FOUND;

            if (env_node->type != OSS_PROC_ENV_NODE || !env_node->priv)
                return HNDL_INVAL;

            env = env_node->priv;
            break;
        }

        init_khandle(&handle, &type, env);
        break;
    }
    case HNDL_TYPE_SYSVAR: {
        user_profile_t* c_profile;
        penv_t* c_env;
        sysvar_t* c_var;

        c_env = c_proc->m_env;
        c_profile = c_env->profile;

        c_var = sysvar_get(c_env->node, path);

        if (!c_var)
            c_var = sysvar_get(c_profile->node, path);

        if (!c_var)
            return HNDL_NOT_FOUND;

        init_khandle(&handle, &type, c_var);
        break;
    }
    case HNDL_TYPE_EVENT: {
        struct kevent* event;

        event = kevent_get(path);

        if (!event)
            return HNDL_NOT_FOUND;

        init_khandle(&handle, &type, event);
        break;
    }
    case HNDL_TYPE_EVENTHOOK:
        break;
    case HNDL_TYPE_SHARED_LIB: {
        /* We (the kernel) does not need to know about how dynamic_library memory is ordered */
        dynamic_library_t* lib_addr = NULL;

        if ((driver_send_msg_a(DYN_LDR_URL, DYN_LDR_LOAD_LIB, (void*)path, sizeof(path), &lib_addr, sizeof(void*))) || !lib_addr)
            return HNDL_NOT_FOUND;

        init_khandle(&handle, &type, lib_addr);
        break;
    }
    case HNDL_TYPE_FS_ROOT:
    case HNDL_TYPE_KOBJ:
        break;
    case HNDL_TYPE_OSS_OBJ: {
        oss_obj_t* obj = NULL;

        if (KERR_ERR(oss_resolve_obj(path, &obj)) || !obj)
            return HNDL_NOT_FOUND;

        init_khandle(&handle, &type, obj);
        break;
    }
    case HNDL_TYPE_NONE:
    default:
        kernel_panic("Tried to open unimplemented handle!");
        break;
    }

    if (!handle.reference.kobj)
        return HNDL_NOT_FOUND;

    /* Clear state bits */
    khandle_set_flags(&handle, flags);

    /* Copy the handle into the map */
    error = bind_khandle(&c_proc->m_handle_map, &handle, (uint32_t*)&ret);

    if (error)
        ret = HNDL_NOT_FOUND;

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

/*!
 * @brief Open a handle to a profile variable from a certain profile handle
 *
 * We don't check for any permissions here, since anyone may recieve a handle to any profile or any profile handle
 * (except if its hidden or something else prevents the opening of pvars)
 */
HANDLE sys_open_pvar(const char* __user name, HANDLE profile_handle, uint16_t flags)
{
    HANDLE ret;
    kerror_t error;
    proc_t* current_proc;
    sysvar_t* svar;
    oss_node_t* target_node;
    khandle_t* khndl;
    khandle_t pvar_khndl;
    khandle_type_t type = HNDL_TYPE_SYSVAR;

    current_proc = get_current_proc();

    /* Validate pointers */
    if (!current_proc || kmem_validate_ptr(current_proc, (uint64_t)name, 1))
        return HNDL_INVAL;

    /* Grab the kernel handle to the target profile */
    khndl = find_khandle(&current_proc->m_handle_map, profile_handle);

    if (!khndl)
        return HNDL_INVAL;

    target_node = nullptr;

    switch (khndl->type) {
    case HNDL_TYPE_PROFILE:
        target_node = khndl->reference.profile->node;
        break;
    case HNDL_TYPE_PROC_ENV:
        target_node = khndl->reference.penv->node;
        break;
    default:
        break;
    }

    if (!target_node)
        return HNDL_NOT_FOUND;

    svar = sysvar_get(target_node, name);

    if (!svar)
        return HNDL_NOT_FOUND;

    if (!oss_obj_can_proc_access(svar->obj, current_proc))
        return HNDL_NOT_FOUND;

    /* Create a kernel handle */
    init_khandle(&pvar_khndl, &type, svar);

    /* Set the flags we want */
    khandle_set_flags(&pvar_khndl, flags);

    /* Copy the handle into the map */
    error = bind_khandle(&current_proc->m_handle_map, &pvar_khndl, (uint32_t*)&ret);

    if (error)
        return HNDL_INVAL;

    return ret;
}

HANDLE sys_open_proc(const char* __user name, uint16_t flags, uint32_t mode)
{
    kernel_panic("Tried to open driver!");
    return HNDL_INVAL;
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

    /* Can't write to this handle =/ */
    if ((khandle->flags & HNDL_FLAG_WRITEACCESS) != HNDL_FLAG_WRITEACCESS)
        return false;

    if (KERR_ERR(sysvar_attach(target_node, name, target_prv_lvl, type, flags, (uintptr_t)value)))
        return false;

    return true;
}
