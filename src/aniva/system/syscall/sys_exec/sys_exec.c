#include "sys_exec.h"
#include "lightos/syscall.h"
#include "dev/core.h"
#include "libk/flow/error.h"
#include "mem/kmem_manager.h"
#include "proc/proc.h"
#include <libk/string.h>
#include "sched/scheduler.h"

#include "drivers/env/kterm/kterm.h"

/*
 * TODO: this function should be redone, since the existance of kterm is not a given
 */
uintptr_t sys_exec(char __user* cmd, size_t cmd_len)
{
  //kernel_panic("Reached a sys_exec syscall");
  proc_t* current_proc;

  if (!cmd)
    return SYS_INV;

  current_proc = get_current_proc();

  if (IsError(kmem_validate_ptr(current_proc, (uintptr_t)cmd, cmd_len)))
      return SYS_INV;

  if (strcmp("clear", cmd) == 0) {
    driver_send_msg("other/kterm", KTERM_DRV_CLEAR, NULL, NULL);
  }

  return SYS_INV;
}

/*!
 * @brief: Get the number of ms since a process launch
 *
 */
uintptr_t sys_get_process_time()
{
  proc_t* curr_prc;

  curr_prc = get_current_proc();

  if (!curr_prc)
    return SYS_INV;

  return curr_prc->m_ticks_elapsed;
}

/*!
 * @brief: Block a process for @ms milliseconds
 */
uintptr_t sys_sleep(uintptr_t ms)
{
  scheduler_yield();
  return 0;
}
