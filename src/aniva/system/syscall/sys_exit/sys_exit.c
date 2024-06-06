#include "sys_exit.h"
#include "libk/flow/error.h"
#include "logging/log.h"
#include "proc/proc.h"
#include "proc/thread.h"
#include "sched/scheduler.h"

/*
 * FIXME: what if a sub-thread of a certain process calls exit() ?
 *
 * NOTE: Refactor to make more readable
 */
uintptr_t sys_exit_handler(uintptr_t code) 
{
  KLOG_DBG("(sys_exit) Thread terminated with code: %d\n", code);

  proc_t* current_proc;
  thread_t* current_thread;

  current_proc = get_current_proc();

  ASSERT_MSG(current_proc, "No current proc to exit! (sys_exit_handler)");

  current_thread = get_current_scheduling_thread();

  ASSERT_MSG(current_thread, "No current thread in the process");
  ASSERT_MSG(current_thread->m_parent_proc == current_proc, "Process to Thread mismatch!");

  /* In this case we may always kill the entire process */
  if (current_thread == current_proc->m_init_thread) {
    goto exit_and_terminate;
  } 

  /* There are more threads in this process */
  if (current_proc->m_thread_count > 1) {
    thread_set_state(current_thread, DYING);

    goto exit_and_yield;
  }

  /*
   * This process was a shared library or a socket that finished initilization and we are waiting on 
   * the signal for destruction
   */
  if (current_proc->m_flags & PROC_SHOULD_STALL) {

    current_proc->m_flags |= PROC_STALLED | PROC_IDLE;

    /* This thread will be idle forever */
    thread_set_state(current_thread, BLOCKED);

    goto exit_and_yield;
  }

exit_and_terminate:
  KLOG_DBG("(sys_exit) Terminating process\n");
  Must(try_terminate_process(current_proc));

exit_and_yield:

  scheduler_yield();

  kernel_panic("Somehow returned to an exited process?");
}
