#include "idle.h"
#include "proc/proc.h"
#include "proc/thread.h"
#include "sched/scheduler.h"
#include "system/profile/profile.h"

proc_t* __kernel_idle_proc;
thread_t* __kernel_idle_thread;

void init_kernel_idle()
{
    __kernel_idle_proc = create_proc("kernel_idle", get_admin_profile(), PF_KERNEL);

    __kernel_idle_thread = create_thread(__generic_proc_idle, NULL, "main", __kernel_idle_proc, true);

    proc_add_thread(__kernel_idle_proc, __kernel_idle_thread);
}

void __generic_proc_idle()
{
    for (;;) {
        // KLOG_DBG("Idling...\n");
        scheduler_yield();
    }
}
