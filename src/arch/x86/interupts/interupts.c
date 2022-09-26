#include "interupts.h"
#include "arch/x86/interupts/idt.h"
#include "libc/string.h"
#include <arch/x86/interupts/control/pic.h>
#include <arch/x86/dev/debug/serial.h>
#include <libc/stddef.h>

static irq_specific_handler_t handler_entries[16] = { NULL };

extern  registers_t* _isr0(registers_t*);
extern  registers_t* _isr1(registers_t*);
extern  registers_t* _isr2(registers_t*);
extern  registers_t* _isr3(registers_t*);
extern  registers_t* _isr4(registers_t*);
extern  registers_t* _isr5(registers_t*);
extern  registers_t* _isr6(registers_t*);
extern  registers_t* _isr7(registers_t*);
extern  registers_t* _isr8(registers_t*);
extern  registers_t* _isr9(registers_t*);
extern  registers_t* _isr10(registers_t*);
extern  registers_t* _isr11(registers_t*);
extern  registers_t* _isr12(registers_t*);
extern  registers_t* _isr13(registers_t*);
extern  registers_t* _isr14(registers_t*);
extern  registers_t* _isr15(registers_t*);
extern  registers_t* _isr16(registers_t*);
extern  registers_t* _isr17(registers_t*);
extern  registers_t* _isr18(registers_t*);
extern  registers_t* _isr19(registers_t*);
extern  registers_t* _isr20(registers_t*);
extern  registers_t* _isr21(registers_t*);
extern  registers_t* _isr22(registers_t*);
extern  registers_t* _isr23(registers_t*);
extern  registers_t* _isr24(registers_t*);
extern  registers_t* _isr25(registers_t*);
extern  registers_t* _isr26(registers_t*);
extern  registers_t* _isr27(registers_t*);
extern  registers_t* _isr28(registers_t*);
extern  registers_t* _isr29(registers_t*);
extern  registers_t* _isr30(registers_t*);
extern  registers_t* _isr31(registers_t*);

extern  registers_t* irq0(registers_t*);
extern  registers_t* irq1(registers_t*);
extern  registers_t* irq2(registers_t*);
extern  registers_t* irq3(registers_t*);
extern  registers_t* irq4(registers_t*);
extern  registers_t* irq5(registers_t*);
extern  registers_t* irq6(registers_t*);
extern  registers_t* irq7(registers_t*);
extern  registers_t* irq8(registers_t*);
extern  registers_t* irq9(registers_t*);
extern  registers_t* irq10(registers_t*);
extern  registers_t* irq11(registers_t*);
extern  registers_t* irq12(registers_t*);
extern  registers_t* irq13(registers_t*);
extern  registers_t* irq14(registers_t*);
extern  registers_t* irq15(registers_t*);

void init_interupts() {

    idt_set_gate(0,  _isr0,  0x08, 0x8E, 0);
	idt_set_gate(1,  _isr1,  0x08, 0x8E, 0);
	idt_set_gate(2,  _isr2,  0x08, 0x8E, 0);
	idt_set_gate(3,  _isr3,  0x08, 0x8E, 0);
	idt_set_gate(4,  _isr4,  0x08, 0x8E, 0);
	idt_set_gate(5,  _isr5,  0x08, 0x8E, 0);
	idt_set_gate(6,  _isr6,  0x08, 0x8E, 0);
	idt_set_gate(7,  _isr7,  0x08, 0x8E, 0);
	idt_set_gate(8,  _isr8,  0x08, 0x8E, 0);
	idt_set_gate(9,  _isr9,  0x08, 0x8E, 0);
	idt_set_gate(10, _isr10, 0x08, 0x8E, 0);
	idt_set_gate(11, _isr11, 0x08, 0x8E, 0);
	idt_set_gate(12, _isr12, 0x08, 0x8E, 0);
	idt_set_gate(13, _isr13, 0x08, 0x8E, 0);
	idt_set_gate(14, _isr14, 0x08, 0x8E, 0);
	idt_set_gate(15, _isr15, 0x08, 0x8E, 0);
	idt_set_gate(16, _isr16, 0x08, 0x8E, 0);
	idt_set_gate(17, _isr17, 0x08, 0x8E, 0);
	idt_set_gate(18, _isr18, 0x08, 0x8E, 0);
	idt_set_gate(19, _isr19, 0x08, 0x8E, 0);
	idt_set_gate(20, _isr20, 0x08, 0x8E, 0);
	idt_set_gate(21, _isr21, 0x08, 0x8E, 0);
	idt_set_gate(22, _isr22, 0x08, 0x8E, 0);
	idt_set_gate(23, _isr23, 0x08, 0x8E, 0);
	idt_set_gate(24, _isr24, 0x08, 0x8E, 0);
	idt_set_gate(25, _isr25, 0x08, 0x8E, 0);
	idt_set_gate(26, _isr26, 0x08, 0x8E, 0);
	idt_set_gate(27, _isr27, 0x08, 0x8E, 0);
	idt_set_gate(28, _isr28, 0x08, 0x8E, 0);
	idt_set_gate(29, _isr29, 0x08, 0x8E, 0);
	idt_set_gate(30, _isr30, 0x08, 0x8E, 0);
	idt_set_gate(31, _isr31, 0x08, 0x8E, 0);
	idt_set_gate(32, irq0,  0x08, 0x8E, 0);
	idt_set_gate(33, irq1,  0x08, 0x8E, 0);
	idt_set_gate(34, irq2,  0x08, 0x8E, 0);
	idt_set_gate(35, irq3,  0x08, 0x8E, 0);
	idt_set_gate(36, irq4,  0x08, 0x8E, 0);
	idt_set_gate(37, irq5,  0x08, 0x8E, 0);
	idt_set_gate(38, irq6,  0x08, 0x8E, 0);
	idt_set_gate(39, irq7,  0x08, 0x8E, 0);
	idt_set_gate(40, irq8,  0x08, 0x8E, 0);
	idt_set_gate(41, irq9,  0x08, 0x8E, 0);
	idt_set_gate(42, irq10, 0x08, 0x8E, 0);
	idt_set_gate(43, irq11, 0x08, 0x8E, 0);
	idt_set_gate(44, irq12, 0x08, 0x8E, 0);
	idt_set_gate(45, irq13, 0x08, 0x8E, 0);
	idt_set_gate(46, irq14, 0x08, 0x8E, 0);
	idt_set_gate(47, irq15, 0x08, 0x8E, 0);
   
}

// FIXME: look into the idea of having a error datastruct for this type of stuff...
// otherwise return an int with statuscode?
void add_handler(size_t irq_num, irq_specific_handler_t handler_ptr) {
    irq_specific_handler_t handler = handler_entries[irq_num];
    if (handler != nullptr){
        // yikes, there already is a handler
        println("handler already exists");
        return;
    }

    if (&handler == &handler_ptr) {
        // yikes
        println("Same handler =/");
        return;
    }

    println("added handler");
    handler_entries[irq_num] = handler_ptr;
}

void remove_handler(size_t irq_num) {
    irq_specific_handler_t handler = handler_entries[irq_num];
    if (!handler) {
        // yikes
        return;
    }

    handler_entries[irq_num] = nullptr;
}

// Traffic distribution function: finds the right handler for the right function
static registers_t* _int_handler(struct registers *regs, int num) {
    irq_specific_handler_t handler = handler_entries[num - 32];

    if (regs) {}
    if (!handler) {
        // ack and return
        println("no handler");
        goto fail;
    }

    // TODO

    if (handler(regs)) {
        pic_eoi(num);
        return regs;
    }
fail:
    pic_eoi(num);
    return regs;
}

// main entrypoint for the interupts (from the asm)
registers_t* interupt_handler (struct registers* regs) {
    // TODO: call _int_handler
    println("yeyz");
    _int_handler(regs, regs->int_no);
    return regs;
}

void disable_interupts() {
    asm volatile ("cli");
}

void enable_interupts() {
    asm volatile ("sti");
}
