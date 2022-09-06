#ifndef __INTERUPTS__
#define __INTERUPTS__

#include <libc/stddef.h>

// NOTE: this file CAN NOT include idt.h, because of an include loop =/

extern struct registers* _irq32 (struct registers* regs);
extern struct registers* _irq33 (struct registers* regs);
extern struct registers* _irq34 (struct registers* regs);
extern struct registers* _irq35 (struct registers* regs);
extern struct registers* _irq36 (struct registers* regs);
extern struct registers* _irq37 (struct registers* regs);
extern struct registers* _irq38 (struct registers* regs);
extern struct registers* _irq39 (struct registers* regs);
extern struct registers* _irq40 (struct registers* regs);
extern struct registers* _irq41 (struct registers* regs);
extern struct registers* _irq42 (struct registers* regs);
extern struct registers* _irq43 (struct registers* regs);
extern struct registers* _irq44 (struct registers* regs);
extern struct registers* _irq45 (struct registers* regs);
extern struct registers* _irq46 (struct registers* regs);
extern struct registers* _irq47 (struct registers* regs);

typedef struct registers{
	uintptr_t r15, r14, r13, r12;
	uintptr_t r11, r10, r9, r8;
	uintptr_t rbp, rdi, rsi, rdx, rcx, rbx, rax;

	uintptr_t int_no, err_code;

	uintptr_t rip, cs, rflags, rsp, ss;
} registers_t;

// handler func ptr
//
typedef struct registers* (*irq_handler_t)(struct registers*);

// specific handler
typedef int (*irq_specific_handler_t) (struct registers*);

// add
void add_handler (size_t irq_num, irq_specific_handler_t handler_ptr);

// remove
void remove_handler (size_t irq_num);

// main entrypoint
struct registers* interupt_handler (struct registers* regs);

// ack (extern?)
extern void interupt_acknowledge (int num);

#endif // !__INTERUPTS__

