#include "scheduler.h"
#include "dev/debug/serial.h"
#include "kmain.h"
#include "libk/error.h"
#include "libk/linkedlist.h"
#include "libk/stddef.h"
#include "libk/string.h"
#include "proc/proc.h"
#include "proc/thread.h"
#include "system/processor/gdt.h"

// FIXME: create a place for this and remove test mark
static void test_kernel_thread_func () {
  for (;;) {
    // hi
  }
}

LIGHT_STATUS init_scheduler() {

  proc_t* proc_ptr = create_kernel_proc(test_kernel_thread_func);

  ASSERT(proc_ptr != nullptr);
  ASSERT(proc_ptr->m_id == 0);

  thread_t* thread = list_get(proc_ptr->m_threads, 0);

  // Make sure we are runnable
  thread->m_current_state = RUNNABLE;

  list_append(g_GlobalSystemInfo.m_current_core->m_processes, proc_ptr);
  g_GlobalSystemInfo.m_current_core->m_current_thread = thread;
  g_GlobalSystemInfo.m_current_core->m_root_thread = thread;

  return LIGHT_FAIL;
}

void enter_scheduler() {

  Processor_t* current = g_GlobalSystemInfo.m_current_core;
  proc_t* proc_ptr = list_get(current->m_processes, 0);

  ASSERT(proc_ptr != nullptr);
  ASSERT(proc_ptr->m_id == 0);

  thread_t* thread = list_get(proc_ptr->m_threads, 0);

  LIGHT_STATUS result = thread_prepare_context(thread);

  if (result == LIGHT_SUCCESS) {
    thread->m_current_state = RUNNING;
  }

  current->m_being_handled_by_scheduler = true;
  kContext_t context = thread->m_context;

  // TODO:
  //current->m_tss.iomap_base = sizeof(tss_entry_t);
  //current->m_tss.rsp0l = context.rsp0 & 0xffffffff;
  //current->m_tss.rsp0h = context.rsp0 >> 32;

  /*
  asm volatile (
    "movq %[_rsp], %%rsp \n"
    "pushq %[thread] \n"
    "pushq %[thread] \n"
    "pushq %[_rip] \n"
    "cld \n"
    //"movq 24(%%rsp), %%rdi \n"
    // scheduler hook
    "retq \n"
    ::  [_rsp] "g" (context.rsp),
        [_rip] "a" (context.rip),
        [thread] "b" (thread)
  );
    */
  switch_thread_context(thread, thread);

  thread_t* t = create_thread(exit_thread, "hji", true);
  thread_prepare_context(t);
  t->m_current_state = RUNNING;
  t->m_cpu = 1;

  switch_thread_context(thread, t);

  kernel_panic("RETURNED FROM enter_scheduler!");
} 

LIGHT_STATUS exit_scheduler() {

  return LIGHT_FAIL;
}

void scheduler_cleanup() {
  Processor_t* current = g_GlobalSystemInfo.m_current_core;

  current->m_being_handled_by_scheduler = false;
}

LIGHT_STATUS switch_context_to(thread_t* thread) {

    return LIGHT_FAIL;
}

void sched_tick(registers_t*);

thread_t* next_thread();
proc_t* next_proc();

LIGHT_STATUS sched_add_proc(proc_t*, proc_id);
LIGHT_STATUS sched_add_thead(thread_t*);

LIGHT_STATUS sched_remove_proc(proc_t*);
LIGHT_STATUS sched_remove_proc_by_id(proc_id);
LIGHT_STATUS sched_remove_thread(thread_t*);

