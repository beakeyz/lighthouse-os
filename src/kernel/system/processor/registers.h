#ifndef __LIGHT_REGISTERS__
#define __LIGHT_REGISTERS__
#include <libk/stddef.h>

typedef struct registers{
	uintptr_t r15, r14, r13, r12;
	uintptr_t r11, r10, r9, r8;
	uintptr_t rbp, rdi, rsi, rdx, rcx, rbx, rax;

	uintptr_t int_no, err_code;

	uintptr_t rip, cs, rflags, rsp, ss;
} __attribute__((packed)) registers_t;


#endif // !__LIGHT_REGISTERS__
