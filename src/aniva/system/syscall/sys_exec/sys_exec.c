
#include "sys_exec.h"
#include "LibSys/syscall.h"
#include "dev/core.h"
#include "dev/kterm/kterm.h"
#include "libk/flow/error.h"
#include "mem/kmem_manager.h"
#include "proc/proc.h"
#include "sched/scheduler.h"

uintptr_t sys_exec(char __user* cmd, size_t cmd_len)
{
  //kernel_panic("Reached a sys_exec syscall");
  proc_t* current_proc;

  if (!cmd)
    return SYS_INV;

  current_proc = get_current_proc();

  if (IsError(kmem_validate_ptr(current_proc, (uintptr_t)cmd, cmd_len)))
      return SYS_INV;

  if (strcmp(cmd, "clear") == 0) {
    Must(driver_send_packet("other/kterm", KTERM_DRV_CLEAR, NULL, NULL));
  }

  return SYS_INV;
}
