#ifndef __ANIVA_REGISTERS__
#define __ANIVA_REGISTERS__
#include <libk/stddef.h>

typedef struct registers {
	uintptr_t rdi;
  uintptr_t rsi;
  uintptr_t rbp;
  uintptr_t rsp;
  uintptr_t rdx;
  uintptr_t rcx;
  uintptr_t rbx;
  uintptr_t rax;
  uintptr_t r8;
  uintptr_t r9; 
  uintptr_t r10;
	uintptr_t r11; 
  uintptr_t r12;
  uintptr_t r13;
  uintptr_t r14;
	uintptr_t r15;

	uintptr_t isr_no, err_code;

	uintptr_t rip, cs, rflags, us_rsp, ss;
} __attribute__((packed)) registers_t;


#endif // !__ANIVA_REGISTERS__
