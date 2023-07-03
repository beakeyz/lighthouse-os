#include "sys_exit.h"
#include "dev/debug/serial.h"
#include "libk/error.h"
#include "libk/string.h"
#include "proc/proc.h"
#include "proc/thread.h"
#include "sched/scheduler.h"

uintptr_t sys_exit_handler(uintptr_t code) {
  proc_t* current_proc;
  thread_t* current_thread;

  current_proc = get_current_proc();

  ASSERT_MSG(current_proc, "No current proc to exit! (sys_exit_handler)");

  current_thread = get_current_scheduling_thread();

  ASSERT_MSG(current_thread, "No current thread in the process");
  ASSERT_MSG(current_thread->m_parent_proc == current_proc, "Process to Thread mismatch!");

  /*
   * This process was a shared library or a socket that finished initilization and we are waiting on 
   * the signal for destruction
   */
  if (current_proc->m_flags & PROC_SHOULD_STALL) {

    current_proc->m_flags |= PROC_STALLED | PROC_IDLE;

    /* This thread will be idle forever */
    // thread_set_state(current_thread, IDLE);
  } else 
    try_terminate_process(current_proc);

  print("(debug) Process terminated with code: ");
  println(to_string(code));

  scheduler_yield();

  kernel_panic("Somehow returned to an exited process?");
}
