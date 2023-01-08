#ifndef __LIGHT_THREAD__
#define __LIGHT_THREAD__
#include "mem/pml.h"
#include "system/processor/gdt.h"
#include <system/processor/registers.h>
#include <libk/stddef.h>

typedef enum {
  INVALID = 0,
  RUNNING,
  RUNNABLE,
  DYING,
  DEAD,
  STOPPED,
  BLOCKED,
} ThreadState;

typedef struct {
  uintptr_t rdi;
  uintptr_t rsi;
  uintptr_t rbp;
  uintptr_t rsp;
  uintptr_t rbx;
  uintptr_t rdx;
  uintptr_t rcx;
  uintptr_t rax;
  uintptr_t r8;
  uintptr_t r9;
  uintptr_t r10;
  uintptr_t r11;
  uintptr_t r12;
  uintptr_t r13;
  uintptr_t r14;
  uintptr_t r15;
  uintptr_t rip;
  uintptr_t rsp0;
  uintptr_t cs;

  uintptr_t rflags;

  uintptr_t cr3;
} x86_thread_registers_t;

typedef struct {
  x86_thread_registers_t m_registers;

  uint32_t m_cpu; // atomic?
  uint32_t m_ticks_elapsed;
  uint32_t m_max_ticks;

  uintptr_t m_stack_top;
} thread_t;

ALWAYS_INLINE void set_flags(x86_thread_registers_t* regs, uintptr_t flags) {
  regs->rflags = flags;
}

ALWAYS_INLINE x86_thread_registers_t setup_regs(bool kernel, pml_t* root_table, uintptr_t stack_top) {
  x86_thread_registers_t regs = {0};

  regs.cs = (kernel ? GDT_KERNEL_CODE : GDT_USER_CODE);

  regs.cr3 = root_table->raw_bits;

  if (kernel) {
    regs.rsp = stack_top;
  }
  regs.rsp0 = stack_top;

  set_flags(&regs, 0x0202);

  return regs;
  
}

#endif // !__LIGHT_THREAD__
