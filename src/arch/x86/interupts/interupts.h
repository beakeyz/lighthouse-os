#ifndef __INTERUPTS__
#define __INTERUPTS__

#include <libc/stddef.h>

// NOTE: this file CAN NOT include idt.h, because of an include loop =/

extern struct registers* _isr0 (struct registers* regs);
extern struct registers* _isr1 (struct registers* regs);
extern struct registers* _isr2 (struct registers* regs);
extern struct registers* _isr3 (struct registers* regs);
extern struct registers* _isr4 (struct registers* regs);
extern struct registers* _isr5 (struct registers* regs);
extern struct registers* _isr6 (struct registers* regs);
extern struct registers* _isr7 (struct registers* regs);
extern struct registers* _isr8 (struct registers* regs);
extern struct registers* _isr9 (struct registers* regs);
extern struct registers* _isr10 (struct registers* regs);
extern struct registers* _isr11 (struct registers* regs);
extern struct registers* _isr12 (struct registers* regs);
extern struct registers* _isr13 (struct registers* regs);
extern struct registers* _isr14 (struct registers* regs);
extern struct registers* _isr15 (struct registers* regs);
extern struct registers* _isr16 (struct registers* regs);
extern struct registers* _isr17 (struct registers* regs);
extern struct registers* _isr18 (struct registers* regs);
extern struct registers* _isr19 (struct registers* regs);
extern struct registers* _isr20 (struct registers* regs);
extern struct registers* _isr21 (struct registers* regs);
extern struct registers* _isr22 (struct registers* regs);
extern struct registers* _isr23 (struct registers* regs);
extern struct registers* _isr24 (struct registers* regs);
extern struct registers* _isr25 (struct registers* regs);
extern struct registers* _isr26 (struct registers* regs);
extern struct registers* _isr27 (struct registers* regs);
extern struct registers* _isr28 (struct registers* regs);
extern struct registers* _isr29 (struct registers* regs);
extern struct registers* _isr30 (struct registers* regs);
extern struct registers* _isr31 (struct registers* regs);
extern struct registers* _isr32 (struct registers* regs);
extern struct registers* _isr33 (struct registers* regs);
extern struct registers* _isr34 (struct registers* regs);
extern struct registers* _isr35 (struct registers* regs);
extern struct registers* _isr36 (struct registers* regs);
extern struct registers* _isr37 (struct registers* regs);
extern struct registers* _isr38 (struct registers* regs);
extern struct registers* _isr39 (struct registers* regs);
extern struct registers* _isr40 (struct registers* regs);
extern struct registers* _isr41 (struct registers* regs);
extern struct registers* _isr42 (struct registers* regs);
extern struct registers* _isr43 (struct registers* regs);
extern struct registers* _isr44 (struct registers* regs);
extern struct registers* _isr45 (struct registers* regs);
extern struct registers* _isr46 (struct registers* regs);
extern struct registers* _isr47 (struct registers* regs);

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

// add
void add_handler (size_t irq_num, irq_specific_handler_t handler_ptr);

// remove
void remove_handler (size_t irq_num);

// main entrypoint
struct registers* interupt_handler (struct registers* regs);

// ack (this should prob be specific to the chip we use lol)
// extern void interupt_acknowledge (int num);

void disable_interupts ();
void enable_interupts ();

#endif // !__INTERUPTS__

