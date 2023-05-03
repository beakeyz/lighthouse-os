#ifndef __ANIVA_KERNEL_CONTEXT__
#define __ANIVA_KERNEL_CONTEXT__
#include "mem/pg.h"
#include "system/processor/gdt.h"
#include <libk/stddef.h>

#define EFLAGS_CF (0)
#define EFLAGS_RES (1 << 1)
#define EFLAGS_PF (1 << 2)
#define EFLAGS_AF (1 << 4)
#define EFLAGS_ZF (1 << 6)
#define EFLAGS_SF (1 << 7)
#define EFLAGS_TF (1 << 8)
#define EFLAGS_IF (1 << 9)
#define EFLAGS_DF (1 << 10)
#define EFLAGS_OF (1 << 11)
#define EFLAGS_IOPL ((1 << 12) | (1 << 13))
#define EFLAGS_NT (1 << 14)
#define EFLAGS_RF (1 << 16)
#define EFLAGS_VM (1 << 17)
#define EFLAGS_AC (1 << 18)
#define EFLAGS_VIF (1 << 19)
#define EFLAGS_VIP (1 << 20)
#define EFLAGS_ID (1 << 21)

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

ALWAYS_INLINE kContext_t setup_regs(bool kernel, pml_entry_t* root_table, uintptr_t stack_top) {
  kContext_t regs = {0};

  //regs.cs = GDT_USER_CODE | 3;
  regs.rflags = 0x0202;
  regs.rsp0 = stack_top;
  regs.cr3 = (uintptr_t)root_table;

    regs.cs = GDT_KERNEL_CODE;
  if (kernel) {
    regs.cs = GDT_KERNEL_CODE;
    regs.rsp = stack_top;
  }

  return regs;
}

// TODO: user regs

ALWAYS_INLINE void contex_set_rip(kContext_t* ctx, uintptr_t rip, uintptr_t arg0, uintptr_t arg1) {
  ctx->rip = rip;
  ctx->rdi = arg0;
  ctx->rsi = arg1;
}

#endif // !__ANIVA_KERNEL_CONTEXT__
