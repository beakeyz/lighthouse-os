#include "thread.h"
#include "dev/debug/serial.h"
#include "kmain.h"
#include "mem/kmem_manager.h"
#include <mem/kmalloc.h>
#include "proc/proc.h"
#include "system/msr.h"
#include "libk/stack.h"
#include "sched/scheduler.h"
#include <libk/string.h>

// TODO: move somewhere central
#define DEFAULT_STACK_SIZE (16 * Kib)

static __attribute__((naked)) void common_thread_entry(void) __attribute__((used));
extern void first_ctx_init(thread_t *from, thread_t *to, registers_t *regs) __attribute__((used));
extern void thread_exit_init_state(thread_t *from) __attribute__((used));

thread_t *create_thread(FuncPtr entry, uintptr_t data, char name[32], bool kthread) { // make this sucka
  thread_t *thread = kmalloc(sizeof(thread_t));
  strcpy(thread->m_name, name);
  thread->m_cpu = get_current_processor()->m_cpu_num;
  thread->m_parent_proc = nullptr;
  thread->m_ticks_elapsed = 0;
  thread->m_max_ticks = SCHED_DEFAULT_PROC_START_TICKS;

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

  thread_set_state(from, RUNNABLE);
  thread_set_state(to, RUNNING);
}

// called when a thread is created and enters the scheduler for the first time
ANIVA_STATUS thread_prepare_context(thread_t *thread) {

  thread_set_state(thread, RUNNABLE);

  kContext_t *ctx_ptr = &thread->m_context;

  registers_t _regs = {};
  registers_t* r = &_regs;
  r->rdi = ctx_ptr->rdi;
  r->rsi = ctx_ptr->rsi;
  r->rbp = ctx_ptr->rbp;
  r->rsp = ctx_ptr->rsp;

  r->rdx = ctx_ptr->rdx;
  r->rcx = ctx_ptr->rcx;
  r->rbx = ctx_ptr->rbx;
  r->rax = ctx_ptr->rax;

  r->r8 = ctx_ptr->r8;
  r->r9 = ctx_ptr->r9;
  r->r10 = ctx_ptr->r10;
  r->r11 = ctx_ptr->r11;
  r->r12 = ctx_ptr->r12;
  r->r13 = ctx_ptr->r13;
  r->r14 = ctx_ptr->r14;
  r->r15 = ctx_ptr->r15;
  r->rflags = ctx_ptr->rflags;
  r->rip = ctx_ptr->rip;
  r->cs = ctx_ptr->cs;

  // TODO: check if this is a user thread or a kernel thread
  r->ss = 0;
  r->us_rsp = thread->m_stack_top;

  STACK_PUSH(ctx_ptr->rsp, registers_t, *r);
  print(thread->m_name);
  print(" stack top: ");
  println(to_string(ctx_ptr->rsp));

  ctx_ptr->rsp0 = thread->m_stack_top;
  ctx_ptr->cs = GDT_KERNEL_CODE;
  ctx_ptr->rip = (uintptr_t)common_thread_entry;
  ctx_ptr->rflags = EFLAGS_IF;
  return ANIVA_FAIL;
}

void switch_thread_context(thread_t *from, thread_t *to) {

}

extern void first_ctx_init(thread_t *from, thread_t *to, registers_t *regs) {

}

extern void thread_exit_init_state(thread_t *from) {

  kContext_t *context_ptr = &from->m_context;
  registers_t regs = *(registers_t*)73424;
  regs.rsp = from->m_stack_top;

  println(to_string(regs.rflags));

  asm volatile (
    "pushq %0\n"
    "pushq %1\n"
    "pushq %2\n"
    "pushq %3\n"
    "pushq %4\n"
    "iretq"
    :: "m"(regs.ss), "m"(regs.rsp), "m"(regs.rflags), "m"(regs.cs), "m"(regs.rip)
    );

  kernel_panic("reached thread_exit_init_state");
}

__attribute__((naked)) void common_thread_entry() {
  asm volatile (
    "jmp thread_exit_init_state \n"
  );
}
