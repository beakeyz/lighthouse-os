#include "irq/faults/faults.h"
#include "libk/flow/error.h"
#include "libk/string.h"
#include "logging/log.h"
#include "proc/thread.h"
#include "sched/scheduler.h"
#include "system/asm_specifics.h"

/*
 * FIXME: this handler is going to get called a bunch of 
 * different times when running the system, so this and a 
 * few handlers that serve the same fate should get overlapping
 * and rigid handling.
 */
enum FAULT_RESULT pagefault_handler(const aniva_fault_t* fault, registers_t *regs) 
{
  uintptr_t err_addr = read_cr2();
  uintptr_t cs = regs->cs;
  
  uint32_t error_word = regs->err_code;

  println(" --- PAGEFAULT --- ");
  print("error at ring: ");
  println(to_string(cs & 3));
  printf("error at addr: 0x%llx\n", err_addr);
  if (error_word & 8) {
    println("Reserved!");
  }
  if (error_word & 2) {
    println("write");
  } else {
    println("read");
  }

  println(((error_word & 1) == 1) ? "ProtVi" : "Non-Present");

  thread_t* current_thread = get_current_scheduling_thread();
  proc_t* current_proc = get_current_proc();
  if (current_proc && current_thread) {
    print(current_proc->m_name);
    print(" : ");
    println(current_thread->m_name);
  }

  /* NOTE: tmp debug */
  //print_bitmap();

  if (!current_thread)
    goto panic;

  if (!current_proc)
    goto panic;

  /* Drivers and other kernel processes should get insta-nuked */
  /* FIXME: a driver can be a non-kernel process. Should user drivers meet the same fate as kernel drivers? */
  if ((current_proc->m_flags & PROC_KERNEL) == PROC_KERNEL)
    goto panic;

  if ((current_proc->m_flags & PROC_DRIVER) == PROC_DRIVER)
    goto panic;

  // /* NOTE: This yields and exits the interrupt context cleanly */
  //Must(try_terminate_process(current_proc));
  //return regs;
  return FR_KILL_PROC;

panic:
  kernel_panic("pagefault! (TODO: more info)");
}
