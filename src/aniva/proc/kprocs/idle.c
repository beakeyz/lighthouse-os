#include "idle.h"
#include "proc/proc.h"
#include "proc/thread.h"
#include "sched/scheduler.h"
#include "system/profile/profile.h"

proc_t* __kernel_idle_proc;
thread_t* __kernel_idle_thread;

void init_kernel_idle()
{
    __kernel_idle_proc = create_proc(NULL, get_admin_profile(), "kernel_idle", __generic_proc_idle, NULL, PROC_KERNEL);
    __kernel_idle_thread = __kernel_idle_proc->m_init_thread;
}

void __generic_proc_idle()
{
    for (;;) {
        // KLOG_DBG("Idling...\n");
        scheduler_yield();
    }
}
