#include "sys_exit.h"
#include "libk/flow/error.h"
#include "libk/string.h"
#include "logging/log.h"
#include "proc/proc.h"
#include "proc/thread.h"
#include "sched/scheduler.h"
#include "sync/atomic_ptr.h"

/*
 * FIXME: what if a sub-thread of a certain process calls exit() ?
 *
 * NOTE: Refactor to make more readable
 */
uintptr_t sys_exit_handler(uintptr_t code) 
{
  print("(debug) Thread terminated with code: ");
  println(to_string(code));

  proc_t* current_proc;
  thread_t* current_thread;

  current_proc = get_current_proc();
  current_thread = get_current_scheduling_thread();

  ASSERT_MSG(current_proc, "No current proc to exit! (sys_exit_handler)");
  ASSERT_MSG(current_thread, "No current thread in the process");
  ASSERT_MSG(current_thread->fid.proc_id == current_proc->m_id, "Process to Thread mismatch!");

  /* In this case we may always kill the entire process */
  if (current_thread == current_proc->m_init_thread)
    goto exit_and_terminate;

  /* There are more threads in this process */
  if (atomic_ptr_read(current_proc->m_thread_count) > 1) {
    thread_set_state(current_thread, DYING);

    goto exit_and_yield;
  }

exit_and_terminate:
  println("(debug) Terminate");
  ASSERT_MSG(KERR_OK(try_terminate_process(current_proc)), "Failed to terminate process!");

exit_and_yield:

  println("(debug) Yield");
  scheduler_yield();

  kernel_panic("Somehow returned to an exited process?");
}
