#include "thread.h"
#include "dev/debug/serial.h"
#include "interupts/interupts.h"
#include "kmain.h"
#include "libk/error.h"
#include "libk/stddef.h"
#include "mem/kmem_manager.h"
#include "proc/context.h"
#include "system/processor/fpu/state.h"
#include "system/processor/gdt.h"
#include "system/processor/registers.h"
#include <libk/string.h>
#include <libk/stack.h>
#include <libk/io.h>

// TODO: move somewhere central
#define DEFAULT_STACK_SIZE 16 * Kib

static ALWAYS_INLINE void common_thread_entry(void);

thread_t* create_thread(FuncPtr entry, const char* name, bool kthread) { // make this sucka

  thread_t* thread = kmalloc(sizeof(thread_t));
  memcpy(thread->m_name, name, strlen(name));
  thread->m_cpu = g_GlobalSystemInfo.m_current_core->m_cpu_num;
  thread->m_parent_proc = nullptr;
  thread->m_ticks_elapsed = 0;
  thread->m_current_state = RUNNABLE;

  thread->m_context.rip = (uintptr_t)entry;
  thread->m_context.rdi = NULL;

  uintptr_t stack_bottom = Must(kmem_kernel_alloc_range(DEFAULT_STACK_SIZE, KMEM_CUSTOMFLAG_GET_MAKE, KMEM_FLAG_WRITABLE | KMEM_FLAG_KERNEL));

  thread->m_stack_top = (stack_bottom + DEFAULT_STACK_SIZE);

  store_fpu_state(&thread->m_fpu_state);

  setup_regs(kthread, &g_GlobalSystemInfo.m_current_core->m_page_dir, thread->m_stack_top);

  return thread;
} // make this sucka

LIGHT_STATUS kill_thread(thread_t* thread) {
  return LIGHT_FAIL;
} // kill thread and prepare for context swap to kernel
LIGHT_STATUS kill_thread_if(thread_t* thread, bool condition) {
  return LIGHT_FAIL;
} // kill if condition is met
LIGHT_STATUS clean_thread(thread_t* thread) {
  return LIGHT_FAIL;
} // clean resources thead used

void exit_thread() {
  println("Exiting...");

  thread_t* thread_to_exit = g_GlobalSystemInfo.m_current_core->m_current_thread;

  for (;;) {
    kernel_panic("TODO: exit thread");
  }
}

LIGHT_STATUS thread_prepare_context(thread_t* thread) {

  thread->m_stack_top -= 64;
  kContext_t context = thread->m_context;
  uintptr_t stack_top = thread->m_stack_top;

  // TODO: push exit function

  STACK_PUSH(stack_top, uintptr_t, 0);

  STACK_PUSH(stack_top, uintptr_t, 0);
  
  registers_t return_frame = {0};

  return_frame.r8 = context.r8;
  return_frame.r9 = context.r9;
  return_frame.r10 = context.r10;
  return_frame.r11 = context.r11;
  return_frame.r12 = context.r12;
  return_frame.r13 = context.r13;
  return_frame.r14 = context.r14;
  return_frame.r15 = context.r15;
  return_frame.rdi = context.rdi;
  return_frame.rsi = context.rsi;
  return_frame.rbp = context.rbp;

  return_frame.rax = context.rax;
  return_frame.rbx = context.rbx;
  return_frame.rcx = context.rcx;
  return_frame.rdx = context.rdx;

  return_frame.rip = context.rip;
  return_frame.rsp = NULL;
  return_frame.rflags = context.rflags;
  return_frame.cs = context.cs;
  if ((context.cs & 0x3) != 0) {
    // UP rsp
    return_frame.us_rsp = context.rsp;
    return_frame.ss = GDT_USER_DATA | 0x3;
  } else {
    return_frame.us_rsp = thread->m_stack_top;
    return_frame.ss = 0;
  }

  STACK_PUSH(stack_top, registers_t, return_frame);

  println(to_string(*((uintptr_t*)stack_top - 24)));

  STACK_PUSH(stack_top, uintptr_t, (uintptr_t)&return_frame);
  
  thread->m_context.rip = (uintptr_t)&common_thread_entry;
  thread->m_context.rsp0 = thread->m_stack_top;
  thread->m_context.rsp = stack_top;
  thread->m_context.cs = GDT_KERNEL_CODE;
  return LIGHT_SUCCESS;
}

ALWAYS_INLINE void common_thread_entry(void) {

  println("Hi from common_thread_entry");

  thread_t* from = nullptr;
  thread_t* to = nullptr;
  registers_t* registers = nullptr;

  asm volatile (
    "popq %0 \n"
    "popq %1 \n"
    "popq %2 \n"
    "cld \n"
    : 
    "=m"(from),
    "=m"(to),
    "=g"(registers)
  );

  println(to_string((uintptr_t)from));
  println(to_string((uintptr_t)to));
  println(to_string((uintptr_t)registers));

  println(to_string(from->m_cpu));
  println(to->m_name);
  println(to_string(registers->us_rsp));

  // TODO: run scheduler stuffskies

  for (;;) {} 
}
