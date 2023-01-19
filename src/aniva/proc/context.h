#ifndef __ANIVA_KERNEL_CONTEXT__
#define __ANIVA_KERNEL_CONTEXT__
#include "mem/PagingComplex.h"
#include "system/processor/gdt.h"
#include <libk/stddef.h>

#define EFLAGS_IF (1 << 21) | (1 << 9)

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
} kContext_t;

ALWAYS_INLINE kContext_t setup_regs(bool kernel, PagingComplex_t* root_table, uintptr_t stack_top) {
  kContext_t regs = {0};

  regs.cs = (kernel ? GDT_KERNEL_CODE : GDT_USER_CODE|3);
  regs.cr3 = (uintptr_t)root_table;

  if (kernel) {
    regs.rsp = stack_top;
  }
  regs.rsp0 = stack_top;
  regs.rflags = 0x0202;

  return regs;
}

ALWAYS_INLINE void contex_set_rip(kContext_t* ctx, uintptr_t rip, uintptr_t args) {
  ctx->rip = rip;
  ctx->rdi = args;
}

#endif // !__ANIVA_KERNEL_CONTEXT__
