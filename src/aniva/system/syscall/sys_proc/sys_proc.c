#include "libk/stddef.h"
#include "lightos/api/handle.h"
#include "lightos/api/process.h"
#include "lightos/syscall.h"
#include "lightos/types.h"
#include "oss/object.h"
#include "proc/handle.h"
#include "proc/proc.h"
#include "sched/scheduler.h"
#include "time/core.h"

HANDLE sys_open_proc_obj(const char* key, handle_flags_t flags)
{
    HANDLE ret;
    khandle_t khandle;
    proc_t* c_proc;
    proc_t* target_proc;
    oss_object_t* target = NULL;

    c_proc = get_current_proc();

    target = c_proc->obj;

    /* If the key isn't specified, give a handle to the calling process */
    if (key) {
        target = nullptr;
        target_proc = find_proc(key);

        if (target_proc)
            target = target_proc->obj;
    } else
        /* Make sure to reference the calling processes object as well */
        oss_object_ref(target);

    if (!target)
        return HNDL_INVAL;

    /* Initialize the handle */
    init_khandle_ex(&khandle, HNDL_TYPE_OBJECT, flags.s_flags, target);

    /* Now bind the handle into the processes handle map */
    bind_khandle(&c_proc->m_handle_map, &khandle, (u32*)&ret);

    return ret;
}

HANDLE sys_create_proc(const char __user* cmd, FuncPtr __user entry)
{
    return HNDL_INVAL;
}

error_t sys_destroy_proc(HANDLE proc, u32 flags)
{
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

    return ((time.s_since_boot - curr_prc->proc_launch_time_rel_boot_sec) * 1000) + time.ms_since_last_s;
}

/*!
 * @brief: Gets the exitvector of the current process
 *
 * This is only applicable if we're using the dynldr driver to load dynamic libraries.
 */
error_t sys_get_exitvec(proc_exitvec_t** p_exitvec)
{
    return -ENOIMPL;
}
