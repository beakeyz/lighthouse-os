#ifndef __LIGHT_REGISTERS__
#define __LIGHT_REGISTERS__
#include <libk/stddef.h>

typedef struct registers{
	uintptr_t rdi, rsi, rbp, rsp, rdx, rcx, rbx, rax;
	uintptr_t r15;
  uintptr_t r14;
  uintptr_t r13;
  uintptr_t r12;
	uintptr_t r11; 
  uintptr_t r10;
  uintptr_t r9; 
  uintptr_t r8;

	uint16_t isr_no, err_code;
  uint32_t rsrv;

	uintptr_t rip, cs, rflags, us_rsp, ss;
} __attribute__((packed)) registers_t;


#endif // !__LIGHT_REGISTERS__
