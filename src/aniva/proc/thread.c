#include "thread.h"
#include "dev/debug/serial.h"
#include "kmain.h"
#include "libk/error.h"
#include "mem/kmem_manager.h"
#include "proc/proc.h"
#include "system/asm_specifics.h"
#include "system/msr.h"
#include <libk/string.h>
#include <libk/stack.h>

// TODO: move somewhere central
#define DEFAULT_STACK_SIZE 16 * Kib

static __attribute__((naked)) void common_thread_entry(void) __attribute__((used));
extern void first_ctx_init(thread_t* from, thread_t* to, registers_t* regs) __attribute__((used));
extern void thread_exit_init_state(thread_t* from, thread_t* to, registers_t* regs) __attribute__((used));

thread_t* create_thread(FuncPtr entry, uintptr_t data, const char* name, bool kthread) { // make this sucka
  thread_t* thread = kmalloc(sizeof(thread_t));
  memcpy(thread->m_name, name, strlen(name));
  thread->m_cpu = get_current_processor()->m_cpu_num;
  thread->m_parent_proc = nullptr;
  thread->m_ticks_elapsed = 0;

  thread_set_state(thread, NO_CONTEXT);

  uintptr_t stack_bottom = Must(kmem_kernel_alloc_range(DEFAULT_STACK_SIZE, KMEM_CUSTOMFLAG_GET_MAKE, KMEM_FLAG_WRITABLE | KMEM_FLAG_KERNEL));
  memset((void*)stack_bottom, 0x00, DEFAULT_STACK_SIZE);

  thread->m_stack_top = (stack_bottom + DEFAULT_STACK_SIZE);

  memcpy(&thread->m_fpu_state, &standard_fpu_state, sizeof(FpuState));

  thread->m_context = setup_regs(kthread, &get_current_processor()->m_page_dir, thread->m_stack_top);
  contex_set_rip(&thread->m_context, (uintptr_t)entry, data);

  return thread;
} // make this sucka

void thread_set_state(thread_t* thread, ThreadState state) {
  thread->m_current_state = state;
  // TODO: update thread context(?) on state change
  // TODO: (??) onThreadStateChangeEvent?
}

ANIVA_STATUS kill_thread(thread_t* thread) {
  return ANIVA_FAIL;
} // kill thread and prepare for context swap to kernel
ANIVA_STATUS kill_thread_if(thread_t* thread, bool condition) {
  return ANIVA_FAIL;
} // kill if condition is met
ANIVA_STATUS clean_thread(thread_t* thread) {
  return ANIVA_FAIL;
} // clean resources thead used

void exit_thread() {

  thread_t* thread_to_exit = get_current_processor()->m_current_thread;

  for (;;) {
//    kernel_panic("TODO: exit thread");
  }
}

extern void thread_enter_context(thread_t* from, thread_t* to) {
  println("thread_enter_context");
  println(to_string(from->m_cpu));
  println(to_string(to->m_cpu));
  println("$thread_enter_context");

  ASSERT(to->m_current_state == RUNNABLE);

  Processor_t* current_processor = get_current_processor();

  println(to_string((uintptr_t)&from));
  println(to_string((uintptr_t)&current_processor->m_current_thread));
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

ANIVA_STATUS thread_prepare_context(thread_t* thread) {

  if (thread->m_current_state != NO_CONTEXT) {
    return ANIVA_FAIL;
  }
  thread_set_state(thread, RUNNABLE);

  kContext_t context = thread->m_context;
  uintptr_t stack_top = thread->m_stack_top;

  print("starting stack_top: ");
  println(to_string(stack_top));
  // TODO: push exit function

  STACK_PUSH(stack_top, uintptr_t, (uintptr_t)exit_thread);

  stack_top -= sizeof(registers_t);
  registers_t* return_frame = (registers_t*)stack_top;

  return_frame->r8 = context.r8;
  return_frame->r9 = context.r9;
  return_frame->r10 = context.r10;
  return_frame->r11 = context.r11;
  return_frame->r12 = context.r12;
  return_frame->r13 = context.r13;
  return_frame->r14 = context.r14;
  return_frame->r15 = context.r15;
  return_frame->rdi = context.rdi;
  return_frame->rsi = context.rsi;
  return_frame->rbp = context.rbp;

  return_frame->rax = context.rax;
  return_frame->rbx = context.rbx;
  return_frame->rcx = context.rcx;
  return_frame->rdx = context.rdx;

  return_frame->rip = context.rip;
  return_frame->rsp = NULL;
  return_frame->rflags = context.rflags;
  return_frame->cs = context.cs;
  if ((context.cs & 0x3) != 0) {
    // UP rsp
    return_frame->us_rsp = context.rsp;
    return_frame->ss = GDT_USER_DATA | 0x3;
  } else {
    return_frame->us_rsp = thread->m_stack_top;
    return_frame->ss = 0;
  }

  stack_top -= sizeof(registers_t*);
  *(uintptr_t*)stack_top = stack_top + sizeof(registers_t*);
  // FIXME: return_frame isn't put on the stack

  thread->m_context.rip = (uintptr_t)common_thread_entry;
  thread->m_context.rsp0 = thread->m_stack_top;
  thread->m_context.rsp = stack_top;
  thread->m_context.cs = GDT_KERNEL_CODE;
  return ANIVA_SUCCESS;
}

void switch_thread_context(thread_t* from, thread_t* to) {

  ASSERT(from->m_current_state == RUNNING && to->m_current_state == RUNNABLE);

  Processor_t* current = get_current_processor();

  asm volatile (
    "pushfq \n"
    "pushq %%rbx \n"
    "pushq %%rcx \n"
    "pushq %%rbp \n"
    "pushq %%rsi \n"
    "pushq %%rdi \n"
    "pushq %%r8 \n"
    "pushq %%r9 \n"
    "pushq %%r10 \n"
    "pushq %%r11 \n"
    "pushq %%r12 \n"
    "pushq %%r13 \n"
    "pushq %%r14 \n"
    "pushq %%r15 \n"
    "movq %%rsp, %[from_rsp] \n"
    "leaq 1f(%%rip), %%rbx \n"
    "movq %%rbx, %[from_rip] \n"
    "movq %[to_rsp0], %%rbx \n"
    "movl %%ebx, %[from_tss_rsp_low] \n"
    "shrq $32, %%rbx \n"
    "movl %%ebx, %[from_tss_rsp_high] \n"
    "movq %[to_rsp], %%rsp \n"
    "pushq %[to] \n"
    "pushq %[from] \n"
    "pushq %[to_rip] \n"
    "cld \n"
    "movq 16(%%rsp), %%rsi \n"
    "movq 8(%%rsp), %%rdi \n"
    "jmp thread_enter_context \n"
    "1: \n"
    "popq %%rdx \n"
    "popq %%rax \n"
    "popq %%r15 \n"
    "popq %%r14 \n"
    "popq %%r13 \n"
    "popq %%r12 \n"
    "popq %%r11 \n"
    "popq %%r10 \n"
    "popq %%r9 \n"
    "popq %%r8 \n"
    "popq %%rdi \n"
    "popq %%rsi \n"
    "popq %%rbp \n"
    "popq %%rcx \n"
    "popq %%rbx \n"
    "popfq \n"
    :
    [from_rsp] "=m"(from->m_context.rsp),
    [from_rip] "=m"(from->m_context.rip),
    [from_tss_rsp_low] "=m"(current->m_tss.rsp0l),
    [from_tss_rsp_high] "=m"(current->m_tss.rsp0h),
    "=d" (from),
    "=a" (to)
    :
    [to_rsp] "g"(to->m_context.rsp),
    [to_rip] "g"(to->m_context.rip),
    [to_rsp0] "c"(to->m_context.rsp0),
    [from] "d" (from),
    [to] "a" (to)
    :
    "memory",
    "rbx"
  );
}

extern void first_ctx_init(thread_t* from, thread_t* to, registers_t* regs) {
  println("Context switch!");

  asm volatile (
    "pushq %0\n"
		"pushq %1\n"
		"pushq %2\n"
		"pushq %3\n"
		"pushq %4\n"
		"iretq"
	: : "m"(regs->ss), "m"(regs->rsp), "m"(regs->rflags), "m"(regs->cs), "m"(regs->rip)
  );
}

extern void thread_exit_init_state(thread_t* from, thread_t* to, registers_t* regs) {
  // TODO: diagnostics


  kernel_panic("test");
}

__attribute__((naked)) void common_thread_entry() {
  asm (
      "popq %rdi \n"
      "popq %rsi \n"
      "popq %rdx \n"
      "cld \n"
      "jmp first_ctx_init \n"
    );
}
