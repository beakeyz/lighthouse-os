#include "dev/core.h"
#include "libk/stddef.h"
#include "lightos/api/dynldr.h"
#include "lightos/api/handle.h"
#include "lightos/syscall.h"
#include "lightos/types.h"
#include "proc/proc.h"
#include "sched/scheduler.h"
#include "time/core.h"

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
error_t sys_get_exitvec(dynldr_exit_vector_t** p_exitvec)
{
    return driver_send_msg_a(DYN_LDR_NAME, DYN_LDR_GET_EXIT_VEC, NULL, NULL, p_exitvec, sizeof(dynldr_exit_vector_t*));
}
