
#include "irq/faults/faults.h"
#include "libk/flow/error.h"
#include "proc/proc.h"
#include "proc/thread.h"
#include "sched/scheduler.h"

enum FAULT_RESULT gpf_handler(const aniva_fault_t* fault, registers_t* regs)
{
  proc_t* c_proc;
  thread_t* c_thrd;

  c_proc = get_current_proc();
  c_thrd = get_current_scheduling_thread();

  printf("GPF in (p: %s, t: %s)\n",
      c_proc ? c_proc->m_name : "None",
      c_thrd ? c_thrd->m_name : "None");

  if (!c_proc || !c_thrd)
    goto kpanic;

  /* Can't recover a kernel process sadly */
  if (is_kernel_proc(c_proc))
    goto kpanic;

  return FR_KILL_PROC;
kpanic:
  kernel_panic("GPF panic");
}
