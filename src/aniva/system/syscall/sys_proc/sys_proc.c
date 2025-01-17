#include "dev/core.h"
#include "fs/file.h"
#include "libk/bin/elf.h"
#include "libk/stddef.h"
#include "lightos/api/dynldr.h"
#include "lightos/api/handle.h"
#include "lightos/api/objects.h"
#include "lightos/syscall.h"
#include "lightos/types.h"
#include "mem/kmem.h"
#include "oss/object.h"
#include "oss/path.h"
#include "proc/handle.h"
#include "proc/proc.h"
#include "sched/scheduler.h"
#include "time/core.h"

/*!
 * @brief: Creates a new process from the given parameters
 *
 * Tries to execute a process binary and clones the current process if this fails
 */
static int _sys_do_create(proc_t* c_proc, const char __user* name, FuncPtr __user entry, proc_t** p_proc)
{
    int error;
    file_t* file;
    proc_t* new_proc;
    oss_path_t path = { 0 };

    /* Parse this mofo into an oss path */
    oss_parse_path_ex(name, &path, ' ');

    /* Grab the file specified in the first 'argument' */
    file = file_open(path.subpath_vec[0]);

    if (!file)
        goto clone_and_exit;

    /* Try to create a new process */
    new_proc = elf_exec_64(file, (c_proc->m_flags & PROC_KERNEL) == PROC_KERNEL);

    /* Close the file regardles of exec result */
    file_close(file);

    if (!new_proc)
        goto clone_and_exit;

    *p_proc = new_proc;

    oss_destroy_path(&path);
    return 0;

clone_and_exit:
    error = proc_clone(c_proc, entry, path.subpath_vec[0], p_proc);

    oss_destroy_path(&path);

    return error;
}

HANDLE sys_create_proc(const char __user* cmd, FuncPtr __user entry)
{
    int error;
    HANDLE ret;
    khandle_t khndl = { 0 };
    proc_t* c_proc;
    proc_t* new_proc;

    c_proc = get_current_proc();

    if (kmem_validate_ptr(c_proc, (u64)cmd, sizeof(const char*)) || (entry && kmem_validate_ptr(c_proc, (u64)entry, sizeof(FuncPtr))))
        return HNDL_INVAL;

    // error = proc_clone(c_proc, entry, name, &new_proc);
    error = _sys_do_create(c_proc, cmd, entry, &new_proc);

    /* If something went wrong here, we're mega fucked */
    if (error)
        return HNDL_INVAL;

    /* Initialize this shit */
    init_khandle_ex(&khndl, HNDL_TYPE_OBJECT, HNDL_FLAG_RW, new_proc->obj);

    /* Try to bind this handle */
    error = bind_khandle(&c_proc->m_handle_map, &khndl, (u32*)&ret);

    /* Yikes */
    if (error)
        goto destroy_and_fail;

    /* Schedule the new process */
    error = proc_schedule(new_proc, c_proc->profile, cmd, NULL, NULL, SCHED_PRIO_MID);

    /* Destroy the thing if we failed */
    if (error)
        goto destroy_and_fail;

    return ret;

destroy_and_fail:
    destroy_proc(new_proc);
    return HNDL_INVAL;
}

error_t sys_destroy_proc(HANDLE proc, u32 flags)
{
    khandle_t* proc_handle;
    proc_t* c_proc;
    proc_t* target_proc;

    c_proc = get_current_proc();

    /* Find the kernel handle */
    proc_handle = find_khandle(&c_proc->m_handle_map, proc);

    if (!proc_handle)
        return EINVAL;

    if (proc_handle->type != HNDL_TYPE_OBJECT)
        return EINVAL;

    target_proc = oss_object_unwrap(proc_handle->object, OT_PROCESS);

    if (!target_proc)
        return EINVAL;

    /*
     * Yikes
     * TODO: Add a force flag for admin processes
     * FIXME: Fix permission attributes lol
     */
    // if ((flags & LIGHTOS_PROC_FLAG_FORCE) != LIGHTOS_PROC_FLAG_FORCE || c_proc->m_env->attr.ptype != PROFILE_TYPE_ADMIN)
    // if (pattr_hasperm(&target_proc->m_env->attr, &c_proc->m_env->attr, PATTR_PROC_KILL))
    // return EPERM;

    /*
     * Try to terminate the process
     * NOTE: If we are trying to terminate ourselves, this call won't return
     */
    if (try_terminate_process(target_proc))
        return EINVAL;

    return 0;
}

/*!
 * @brief: Get the number of ms since a process launch
 *
 */
uintptr_t sys_get_process_time()
{
    proc_t* curr_prc;
    system_time_t time;

    curr_prc = get_current_proc();

    if (!curr_prc)
        return EINVAL;

    if (time_get_system_time(&time))
        return 0;

    // KLOG_DBG("Getting system time %d -> %lld\n", time.s_since_boot, time.ms_since_last_s);

    return ((time.s_since_boot - curr_prc->m_dt_since_boot) * 1000) + time.ms_since_last_s;
}

/*!
 * @brief: Gets the exitvector of the current process
 *
 * This is only applicable if we're using the dynldr driver to load dynamic libraries.
 */
error_t sys_get_exitvec(dynldr_exit_vector_t** p_exitvec)
{
    return driver_send_msg_a(DYN_LDR_URL, DYN_LDR_GET_EXIT_VEC, NULL, NULL, p_exitvec, sizeof(dynldr_exit_vector_t*));
}
