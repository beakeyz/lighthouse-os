#include "scheduler.h"
#include "dev/debug/serial.h"
#include "interupts/interupts.h"
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
  println("entering test_kernel_thread_func");
}

ANIVA_STATUS init_scheduler() {

  proc_t* proc_ptr = create_kernel_proc(test_kernel_thread_func);

  ASSERT(proc_ptr != nullptr);
  ASSERT(proc_ptr->m_id == 0);

  thread_t* thread = proc_ptr->m_idle_thread;

  list_append(get_current_processor()->m_processes, proc_ptr);
  get_current_processor()->m_current_thread = thread;
  get_current_processor()->m_root_thread = thread;

  return ANIVA_FAIL;
}

void enter_scheduler() {

  disable_interupts();
  Processor_t* current = get_current_processor();
  proc_t* proc_ptr = list_get(current->m_processes, 0);

  ASSERT(proc_ptr != nullptr);
  ASSERT(proc_ptr->m_id == 0);

  thread_t* thread = proc_ptr->m_idle_thread; 

  ANIVA_STATUS result = thread_prepare_context(thread);

  if (result == ANIVA_SUCCESS) {
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
    "retq \n"
    ::  [_rsp] "g" (context.rsp),
        [_rip] "a" (context.rip),
        [thread] "b" (thread),
        [cpu_id] "c" ((uintptr_t)current->m_cpu_num)
  );

  kernel_panic("RETURNED FROM enter_scheduler!");

} 

ANIVA_STATUS exit_scheduler() {

  return ANIVA_FAIL;
}

extern ALWAYS_INLINE void sched_on_cpu_enter(uint32_t cpu_num) {

  // TODO:
}

void scheduler_cleanup() {
  Processor_t* current = get_current_processor();

  current->m_being_handled_by_scheduler = false;
}

ANIVA_STATUS sched_switch_context_to(thread_t* thread) {
  Processor_t* current_processor = get_current_processor();
  thread_t* from = current_processor->m_current_thread;

  ASSERT(from != nullptr);
  if (from == thread) {
    return ANIVA_FAIL;
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

  return ANIVA_FAIL;
}

void sched_tick(registers_t*);

thread_t* sched_next_thread() {
  return get_current_processor()->m_root_thread;
}
proc_t* sched_next_proc() {
  return nullptr;
}

void sched_next() {
  Processor_t* current_processor = get_current_processor();

  if (!sched_has_next()) {
    kernel_panic("something went very wrong (scheduler)");
  }

  thread_t* n_thread = sched_next_thread();

  // TODO: collect diagnostics on this threads behaviour in order to
  // detirmine how much time this thread gets

  sched_switch_context_to(n_thread);
}

void sched_safe_next() {
  Processor_t* current_processor = get_current_processor();

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

ANIVA_STATUS sched_add_proc(proc_t*, proc_id);
ANIVA_STATUS sched_add_thead(thread_t*);

ANIVA_STATUS sched_remove_proc(proc_t*);
ANIVA_STATUS sched_remove_proc_by_id(proc_id);
ANIVA_STATUS sched_remove_thread(thread_t*);

