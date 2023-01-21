#include "thread.h"
#include "dev/debug/serial.h"
#include "kmain.h"
#include "mem/kmem_manager.h"
#include <mem/kmalloc.h>
#include "proc/proc.h"
#include "system/msr.h"
#include "libk/stack.h"
#include <libk/string.h>

// TODO: move somewhere central
#define DEFAULT_STACK_SIZE (16 * Kib)

static __attribute__((naked)) void common_thread_entry(void) __attribute__((used));

extern void first_ctx_init(thread_t *from, thread_t *to, registers_t *regs) __attribute__((used));

extern void thread_exit_init_state(thread_t *from, thread_t *to, registers_t *regs) __attribute__((used));

thread_t *create_thread(FuncPtr entry, uintptr_t data, char name[32], bool kthread) { // make this sucka
  thread_t *thread = kmalloc(sizeof(thread_t));
  strcpy(thread->m_name, name);
  thread->m_cpu = get_current_processor()->m_cpu_num;
  thread->m_parent_proc = nullptr;
  thread->m_ticks_elapsed = 0;

  thread_set_state(thread, NO_CONTEXT);

  uintptr_t stack_bottom = Must(kmem_kernel_alloc_range(DEFAULT_STACK_SIZE, KMEM_CUSTOMFLAG_GET_MAKE,KMEM_FLAG_WRITABLE | KMEM_FLAG_KERNEL));
  memset((void *) stack_bottom, 0x00, DEFAULT_STACK_SIZE);
  thread->m_stack_top = (stack_bottom + DEFAULT_STACK_SIZE);

  memcpy(&thread->m_fpu_state, &standard_fpu_state, sizeof(FpuState));

  thread->m_context = setup_regs(kthread, &get_current_processor()->m_page_dir, thread->m_stack_top);
  contex_set_rip(&thread->m_context, (uintptr_t) entry, data);

  return thread;
} // make this sucka

// funny wrapper
void thread_set_entrypoint(thread_t* ptr, FuncPtr entry, uintptr_t data) {
  contex_set_rip(&ptr->m_context, (uintptr_t)entry, data);
}

void thread_set_state(thread_t *thread, ThreadState state) {
  thread->m_current_state = state;
  // TODO: update thread context(?) on state change
  // TODO: (??) onThreadStateChangeEvent?
}

ANIVA_STATUS kill_thread(thread_t *thread) {
  return ANIVA_FAIL;
} // kill thread and prepare for context swap to kernel

ANIVA_STATUS kill_thread_if(thread_t *thread, bool condition) {
  return ANIVA_FAIL;
} // kill if condition is me
ANIVA_STATUS clean_thread(thread_t *thread) {
  return ANIVA_FAIL;
} // clean resources thead used

void exit_thread() {

  for (;;) {
//    kernel_panic("TODO: exit thread");
  }
}

// TODO: redo?
extern void thread_enter_context(thread_t *from, thread_t *to) {
  println("thread_enter_context");
  println(to_string(from->m_cpu));
  println(to_string(to->m_cpu));
  println("$thread_enter_context");

  ASSERT(to->m_current_state == RUNNABLE);

  Processor_t *current_processor = get_current_processor();

  println(to_string((uintptr_t) &from));
  println(to_string((uintptr_t) &current_processor->m_current_thread));
  if (current_processor->m_current_thread != from) {
    kernel_panic("Switched from an invalid thread!");
  }

  kContext_t from_context = from->m_context;
  kContext_t to_context = to->m_context;

  current_processor->m_current_thread = to;

  save_fpu_state(&from->m_fpu_state);

  // TODO: tls via FS base? or LDT (linux does this r sm)

  wrmsr(MSR_FS_BASE, 0);

  if (from->m_context.cr3 != to->m_context.cr3)
    write_cr3(to->m_context.cr3);

  // NOTE: for correction purposes
  to->m_cpu = current_processor->m_cpu_num;

  store_fpu_state(&to->m_fpu_state);

  thread_set_state(to, RUNNING);
}

ANIVA_STATUS thread_prepare_context(thread_t *thread) {

  if (thread->m_current_state != NO_CONTEXT) {
    return ANIVA_FAIL;
  }

  registers_t *r = nullptr;
  STACK_PUSH(thread->m_context.rsp, registers_t, *r);



  return ANIVA_FAIL;
}

void switch_thread_context(thread_t *from, thread_t *to) {

}

extern void first_ctx_init(thread_t *from, thread_t *to, registers_t *regs) {

}

extern void thread_exit_init_state(thread_t *from, thread_t *to, registers_t *regs) {

}

__attribute__((naked)) void common_thread_entry() {

}
