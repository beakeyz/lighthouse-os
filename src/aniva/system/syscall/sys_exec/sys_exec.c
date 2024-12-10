#include "dev/core.h"
#include "kterm/shared.h"
#include "lightos/error.h"
#include "lightos/syscall.h"
#include "mem/kmem_manager.h"
#include "proc/proc.h"
#include "sched/scheduler.h"
#include <libk/string.h>

/*
 * TODO: this function should be redone, since the existance of kterm is not a given
 */
error_t sys_exec(const char __user* cmd, size_t cmd_len)
{
    // kernel_panic("Reached a sys_exec syscall");
    proc_t* current_proc;

    if (!cmd)
        return EINVAL;

    current_proc = get_current_proc();

    if (kmem_validate_ptr(current_proc, (uintptr_t)cmd, cmd_len))
        return EINVAL;

    if (strcmp("clear", cmd) == 0)
        driver_send_msg("other/kterm", KTERM_DRV_CLEAR, NULL, NULL);

    return 0;
}

/*!
 * @brief: Block a process for @ms milliseconds
 *
 * TODO:
 */
void sys_sleep(uintptr_t ms)
{
    scheduler_yield();
}
