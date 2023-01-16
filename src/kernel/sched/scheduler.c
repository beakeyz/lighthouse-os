#include "scheduler.h"
#include "dev/debug/serial.h"
#include "kmain.h"
#include "libk/error.h"
#include "libk/linkedlist.h"
#include "libk/stddef.h"
#include "libk/string.h"
#include "mem/kmem_manager.h"
#include "proc/proc.h"
#include "proc/thread.h"
#include "sync/atomic_ptr.h"
#include "system/processor/gdt.h"
#include "system/processor/processor.h"

extern ALWAYS_INLINE void sched_on_cpu_enter(uint32_t cpu_num);

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

  thread_t* thread = proc_ptr->m_idle_thread;

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

  thread_t* thread = proc_ptr->m_idle_thread; 

  LIGHT_STATUS result = thread_prepare_context(thread);

  if (result == LIGHT_SUCCESS) {
    thread->m_current_state = RUNNING;
  } else {
    // yikes
    return;
  }

  current->m_being_handled_by_scheduler = true;
  kContext_t context = thread->m_context;

  // TODO:
  current->m_tss.iomap_base = sizeof(tss_entry_t);
  current->m_tss.rsp0l = context.rsp0 & 0xffffffff;
  current->m_tss.rsp0h = context.rsp0 >> 32;

  asm volatile (
    "movq %[_rsp], %%rsp \n"
    "pushq %[thread] \n"
    "pushq %[thread] \n"
    "pushq %[_rip] \n"
    "cld \n"
    "movq %[cpu_id], %%rdi \n"
    "call sched_on_cpu_enter \n" // hook
    "retq \n"
    ::  [_rsp] "g" (context.rsp),
        [_rip] "a" (context.rip),
        [thread] "b" (thread),
        [cpu_id] "c" ((uintptr_t)current->m_cpu_num)
  );

  kernel_panic("RETURNED FROM enter_scheduler!");
} 

LIGHT_STATUS exit_scheduler() {

  return LIGHT_FAIL;
}

extern ALWAYS_INLINE void sched_on_cpu_enter(uint32_t cpu_num) {
  // TODO:
}

void scheduler_cleanup() {
  Processor_t* current = g_GlobalSystemInfo.m_current_core;

  current->m_being_handled_by_scheduler = false;
}

LIGHT_STATUS sched_switch_context_to(thread_t* thread) {
  Processor_t* current_processor = g_GlobalSystemInfo.m_current_core;
  thread_t* from = current_processor->m_current_thread;

  ASSERT(from != nullptr);
  if (from == thread) {
    return LIGHT_FAIL;
  }

  if (from->m_current_state == RUNNING) {
    thread_set_state(from, RUNNABLE);
  }

  if (thread->m_current_state == NO_CONTEXT) {
    thread_prepare_context(thread);
    thread->m_current_state = RUNNABLE;
  }

  thread->m_current_state = RUNNING;

  switch_thread_context(from, thread);

  ASSERT(thread == current_processor->m_current_thread);

  return LIGHT_FAIL;
}

void sched_tick(registers_t*);

thread_t* sched_next_thread() {
  return g_GlobalSystemInfo.m_current_core->m_root_thread;
}
proc_t* sched_next_proc() {
  return nullptr;
}

void sched_next() {
  Processor_t* current_processor = g_GlobalSystemInfo.m_current_core;

  if (!sched_has_next()) {
    kernel_panic("something went very wrong (scheduler)");
  }

  thread_t* n_thread = sched_next_thread();

  // TODO: collect diagnostics on this threads behaviour in order to
  // detirmine how much time this thread gets

  sched_switch_context_to(n_thread);
}

void sched_safe_next() {
  Processor_t* current_processor = g_GlobalSystemInfo.m_current_core;

  if (!current_processor->m_being_handled_by_scheduler && current_processor->m_current_thread != nullptr) {
    sched_next();
  }
}

bool sched_has_next() {
  // TODO: check if we want another proc to have execution

  return (sched_next_thread() != nullptr);
}

void sched_first_switch_finished() {
}

LIGHT_STATUS sched_add_proc(proc_t*, proc_id);
LIGHT_STATUS sched_add_thead(thread_t*);

LIGHT_STATUS sched_remove_proc(proc_t*);
LIGHT_STATUS sched_remove_proc_by_id(proc_id);
LIGHT_STATUS sched_remove_thread(thread_t*);

