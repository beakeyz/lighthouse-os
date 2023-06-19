#include "sys_exit.h"
#include "dev/debug/serial.h"
#include "libk/error.h"
#include "proc/proc.h"
#include "proc/thread.h"
#include "sched/scheduler.h"

uintptr_t sys_exit_handler(uintptr_t code) {
  proc_t* current_proc;

  current_proc = get_current_proc();

  try_terminate_process(current_proc);

  scheduler_yield();

  kernel_panic("TODO: exit a process");
}
