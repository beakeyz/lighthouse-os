#include "sys_proc.h"
#include "fs/file.h"
#include "libk/bin/elf.h"
#include "libk/stddef.h"
#include "lightos/handle_def.h"
#include "mem/kmem_manager.h"
#include "proc/handle.h"
#include "proc/proc.h"
#include "sched/scheduler.h"
#include <proc/env.h>

/*!
 * @brief: Creates a new process from the given parameters
 *
 * Tries to execute a process binary and clones the current process if this fails
 */
static int _sys_do_create(proc_t* c_proc, const char __user* name, FuncPtr __user entry, proc_t** p_proc)
{
    file_t* file;
    proc_t* new_proc;

    file = file_open(name);

    if (!file)
        goto clone_and_exit;

    /* Try to create a new process */
    new_proc = elf_exec_64(file, (c_proc->m_flags & PROC_KERNEL) == PROC_KERNEL);

    /* Close the file regardles of exec result */
    file_close(file);

    if (!new_proc)
        goto clone_and_exit;

    return 0;

clone_and_exit:
    return proc_clone(c_proc, entry, name, p_proc);
}

u64 sys_create_proc(const char __user* name, FuncPtr __user entry)
{
    int error;
    proc_t* c_proc;
    proc_t* new_proc;

    c_proc = get_current_proc();

    if (kmem_validate_ptr(c_proc, (u64)name, sizeof(const char*)) || (entry && kmem_validate_ptr(c_proc, (u64)entry, sizeof(FuncPtr))))
        return SYS_ERR;

    // error = proc_clone(c_proc, entry, name, &new_proc);
    error = _sys_do_create(c_proc, name, entry, &new_proc);

    /* If something went wrong here, we're mega fucked */
    if (error)
        return SYS_KERR;

    error = proc_schedule(new_proc, c_proc->m_env->profile, NULL, NULL, SCHED_PRIO_MID);

    if (error)
        destroy_proc(new_proc);

    return error ? SYS_KERR : 0;
}

u64 sys_destroy_proc(HANDLE proc, u32 flags)
{
    khandle_t* proc_handle;
    proc_t* c_proc;
    proc_t* target_proc;

    c_proc = get_current_proc();

    /* Find the kernel handle */
    proc_handle = find_khandle(&c_proc->m_handle_map, proc);

    if (!proc_handle)
        return SYS_INV;

    if (proc_handle->type != HNDL_TYPE_PROC)
        return SYS_INV;

    target_proc = proc_handle->reference.process;

    /*
     * Try to terminate the process
     * NOTE: If we are trying to terminate ourselves, this call won't return
     */
    if (try_terminate_process(target_proc))
        return SYS_ERR;

    return 0;
}
