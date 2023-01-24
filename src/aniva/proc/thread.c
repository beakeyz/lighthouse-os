#include "thread.h"
#include "dev/debug/serial.h"
#include "kmain.h"
#include "mem/kmem_manager.h"
#include <mem/kmalloc.h>
#include "proc/proc.h"
#include "system/msr.h"
#include "libk/stack.h"
#include "sched/scheduler.h"
#include "libk/kevent/core.h"
#include "libk/kevent/eventhook.h"
#include <libk/string.h>

// TODO: move somewhere central
#define DEFAULT_STACK_SIZE (16 * Kib)

static __attribute__((naked)) void common_thread_entry(void) __attribute__((used));
extern void first_ctx_init(thread_t *from, thread_t *to, registers_t *regs) __attribute__((used));
extern void thread_exit_init_state(thread_t *from) __attribute__((used));

thread_t *create_thread(FuncPtr entry, uintptr_t data, char name[32], bool kthread) { // make this sucka
  thread_t *thread = kmalloc(sizeof(thread_t));
  thread->m_self = thread;
  strcpy(thread->m_name, name);
  thread->m_cpu = get_current_processor()->m_cpu_num;
  thread->m_parent_proc = nullptr;
  thread->m_ticks_elapsed = 0;
  thread->m_max_ticks = SCHED_DEFAULT_PROC_START_TICKS;
  thread->m_has_been_scheduled = false;

  thread_set_state(thread, NO_CONTEXT);

  uintptr_t stack_bottom = Must(kmem_kernel_alloc_range(DEFAULT_STACK_SIZE, KMEM_CUSTOMFLAG_GET_MAKE,KMEM_FLAG_WRITABLE | KMEM_FLAG_KERNEL));
  memset((void *) stack_bottom, 0x00, DEFAULT_STACK_SIZE);
  thread->m_stack_top = (stack_bottom + DEFAULT_STACK_SIZE);

  print("Thread ( ");
  print(thread->m_name);
  print(" ) creation debug: ");
  println(to_string(thread->m_stack_top));

  memcpy(&thread->m_fpu_state, &standard_fpu_state, sizeof(FpuState));

  thread->m_context = setup_regs(kthread, get_current_processor()->m_page_dir, thread->m_stack_top);
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
    kernel_panic("TODO: exit thread");
  }
}

// TODO: redo?
extern void thread_enter_context(thread_t *to) {
  println("$thread_enter_context");
  println(to->m_name);

  // FIXME: uncomment asap
  //ASSERT(to->m_current_state == RUNNABLE);

  Processor_t *current_processor = get_current_processor();

  current_processor->m_current_thread = to;

  // TODO: make use of this
  //struct context_switch_event_hook hook = create_context_switch_event_hook(to);
  //call_event(CONTEXT_SWITCH_EVENT, &hook);

  // FIXME: tls via FS base? or LDT (linux does this r sm)
  //wrmsr(MSR_FS_BASE, (uintptr_t)to);

  // crashes =/
  //write_cr3(to->m_context.cr3);

  // NOTE: for correction purposes
  to->m_cpu = current_processor->m_cpu_num;

  store_fpu_state(&to->m_fpu_state);

  thread_set_state(to, RUNNING);

  println("done setting up context");
}

// called when a thread is created and enters the scheduler for the first time
ANIVA_STATUS thread_prepare_context(thread_t *thread) {

  thread_set_state(thread, RUNNABLE);
  uintptr_t rsp = thread->m_stack_top;

  STACK_PUSH(rsp, uintptr_t, 0);
  STACK_PUSH(rsp, uintptr_t, (uintptr_t)exit_thread);

  rsp -= sizeof(registers_t);
  registers_t* regs = (registers_t*)rsp;
  regs->rdi = thread->m_context.rdi;
  regs->rsi = thread->m_context.rsi;
  regs->rbp = thread->m_context.rbp;
  regs->rdx = thread->m_context.rdx;
  regs->rcx = thread->m_context.rcx;
  regs->rbx = thread->m_context.rbx;
  regs->rax = thread->m_context.rax;
  regs->r8 = thread->m_context.r8;
  regs->r9 = thread->m_context.r9;
  regs->r10 = thread->m_context.r10;
  regs->r11 = thread->m_context.r11;
  regs->r12 = thread->m_context.r12;
  regs->r13 = thread->m_context.r13;
  regs->r14 = thread->m_context.r14;
  regs->r15 = thread->m_context.r15;
  regs->rip = thread->m_context.rip;
  regs->cs = thread->m_context.cs;
  regs->rflags = thread->m_context.rflags;
  regs->us_rsp = thread->m_stack_top;
  regs->ss = 0;

  thread->m_context.rsp = rsp;
  thread->m_context.rsp0 = thread->m_stack_top;
  thread->m_context.cs = GDT_KERNEL_CODE;
  return ANIVA_FAIL;
}

// used to bootstrap the iret stub created in thread_prepare_context
void thread_enter_context_first_time(thread_t* thread) {

  thread->m_has_been_scheduled = true;

  print("(INIT) Loading context of: ");
  println(thread->m_name);

  ASSERT(thread->m_current_state != NO_CONTEXT);
  thread_set_state(thread, RUNNING);

  tss_entry_t *tss_ptr = &get_current_processor()->m_tss;
  tss_ptr->iomap_base = sizeof(get_current_processor()->m_tss);
  tss_ptr->rsp0l = thread->m_stack_top & 0xffffffff;
  tss_ptr->rsp0h = thread->m_stack_top >> 32;
  println(to_string((uintptr_t)thread));

  asm volatile (
    "movq %[new_rsp], %%rsp \n"
    "movq %[thread], %%rdi \n"
    "call thread_exit_init_state \n"
    "cld \n"
    "popq %%rdi \n"
    "popq %%rsi \n"
    "popq %%rbp \n"
    "popq %%rdx \n"
    "popq %%rcx \n"
    "popq %%rbx \n"
    "popq %%rax \n"
    "popq %%r8  \n"
    "popq %%r9  \n"
    "popq %%r10 \n"
    "popq %%r11 \n"
    "popq %%r12 \n"
    "popq %%r13 \n"
    "popq %%r14 \n"
    "popq %%r15 \n"
    "addq $16, %%rsp \n"
    "iretq \n"
    :
  [tss_rsp0l]"=m"(tss_ptr->rsp0l),
  [tss_rsp0h]"=m"(tss_ptr->rsp0h),
    "=d"(thread)
  :
  [new_rsp]"g"(thread->m_context.rsp),
  [new_rip]"g"(thread->m_context.rip),
  [thread]"d"(thread)
  : "memory"
  );
}

void thread_save_context(thread_t* thread) {

}

void thread_switch_context(thread_t* from, thread_t* to) {

  // FIXME: saving context is still wonky!!!
  // without saving the context it kinda works,
  // but it keep running the function all over again.
  // since no state is saved, is just starts all over

  print("Saving context of: ");
  println(from->m_name);

  thread_set_state(from, RUNNABLE);

  print("Loading context of: ");
  println(to->m_name);
  to->m_has_been_scheduled = true;

  thread_set_state(to, RUNNING);

  //ASSERT(get_current_scheduling_thread() == from);

  tss_entry_t *tss_ptr = &get_current_processor()->m_tss;

  save_fpu_state(&from->m_fpu_state);

  asm volatile (
    "pushfq \n"
    "pushq %%r15 \n"
    "pushq %%r14 \n"
    "pushq %%r13 \n"
    "pushq %%r12 \n"
    "pushq %%r11 \n"
    "pushq %%r10 \n"
    "pushq %%r9 \n"
    "pushq %%r8 \n"
    "pushq %%rax \n"
    "pushq %%rcx \n"
    "pushq %%rbx \n"
    "pushq %%rdx \n"
    "pushq %%rsi \n"
    "pushq %%rbp \n"
    "pushq %%rdi \n"
    "movq %%rsp, %[old_rsp] \n"
    "leaq 1f(%%rip), %%rbx \n"
    "movq %%rbx, %[old_rip] \n"
    "movq %[stack_top], %%rbx \n"
    "movl %%ebx, %[tss_rsp0l] \n"
    "shrq $32, %%rbx \n"
    "movl %%ebx, %[tss_rsp0h] \n"
    "movq %[new_rsp], %%rsp \n"
    "pushq %[thread] \n"
    "pushq %[new_rip] \n"
    "cld \n"
    "movq 8(%%rsp), %%rdi \n"
    "jmp thread_enter_context \n"
    "1: \n"
    "addq $8, %%rsp \n"
    "popq %%rdi \n"
    "popq %%rsi \n"
    "popq %%rbp \n"
    "popq %%rdx \n"
    "popq %%rcx \n"
    "popq %%rbx \n"
    "popq %%rax \n"
    "popq %%r8  \n"
    "popq %%r9  \n"
    "popq %%r10 \n"
    "popq %%r11 \n"
    "popq %%r12 \n"
    "popq %%r13 \n"
    "popq %%r14 \n"
    "popq %%r15 \n"
    "popfq \n"
    :
    [old_rip]"=m"(from->m_context.rip),
    [old_rsp]"=m"(from->m_context.rsp),
    [tss_rsp0l]"=m"(tss_ptr->rsp0l),
    [tss_rsp0h]"=m"(tss_ptr->rsp0h),
      "=d"(to)
    :
    [new_rsp]"g"(to->m_context.rsp),
    [new_rip]"g"(to->m_context.rip),
    [stack_top]"g"(to->m_stack_top),
    [thread]"d"(to)
    : "memory", "rbx"
  );
}

// TODO: this thing
extern void thread_exit_init_state(thread_t *from) {

  //kernel_panic("reached thread_exit_init_state");
}

__attribute__((naked)) void common_thread_entry() {

}
