#ifndef __INTERUPTS__
#define __INTERUPTS__

#include <libc/stddef.h>

// NOTE: this file CAN NOT include idt.h, because of an include loop =/

typedef struct registers{
	uintptr_t r15, r14, r13, r12;
	uintptr_t r11, r10, r9, r8;
	uintptr_t rbp, rdi, rsi, rdx, rcx, rbx, rax;

	uintptr_t int_no, err_code;

	uintptr_t rip, cs, rflags, rsp, ss;
} registers_t;

// handler func ptrs
typedef struct registers* (*irq_handler_t)(struct registers*);
typedef int (*irq_specific_handler_t) (struct registers*);

// init 0.0
void init_interupts();

// add
void add_handler (size_t irq_num, irq_specific_handler_t handler_ptr);

// remove
void remove_handler (size_t irq_num);

// main entrypoint
registers_t* interupt_handler (struct registers* regs);

// ack (this should prob be specific to the chip we use lol)
// extern void interupt_acknowledge (int num);

void disable_interupts ();
void enable_interupts ();

#endif // !__INTERUPTS__

