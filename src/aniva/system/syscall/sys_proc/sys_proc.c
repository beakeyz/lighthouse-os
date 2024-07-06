#include "sys_proc.h"
#include "lightos/handle_def.h"
#include "mem/kmem_manager.h"
#include "proc/handle.h"
#include "proc/proc.h"
#include "sched/scheduler.h"

u64 sys_create_proc(const char __user* name, FuncPtr __user entry)
{
    int error;
    proc_t* c_proc;
    proc_t* new_proc;

    c_proc = get_current_proc();

    if (kmem_validate_ptr(c_proc, (u64)name, sizeof(const char*)) || kmem_validate_ptr(c_proc, (u64)entry, sizeof(FuncPtr)))
        return SYS_ERR;

    error = proc_clone(c_proc, entry, name, &new_proc);

    /* If something went wrong here, we're mega fucked */
    if (error)
        return SYS_KERR;

    error = proc_schedule(new_proc, SCHED_PRIO_MID);

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
