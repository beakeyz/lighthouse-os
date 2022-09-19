#include "interupts.h"
#include "arch/x86/interupts/idt.h"
#include "libc/string.h"
#include <arch/x86/interupts/control/pic.h>
#include <arch/x86/dev/debug/serial.h>
#include <libc/stddef.h>

static irq_specific_handler_t handler_entries[16] = { NULL };

extern  void _isr0();
extern  void _isr1();
extern  void _isr2();
extern  void _isr3();
extern  void _isr4();
extern  void _isr5();
extern  void _isr6();
extern  void _isr7();
extern  void _isr8();
extern  void _isr9();
extern  void _isr10();
extern  void _isr11();
extern  void _isr12();
extern  void _isr13();
extern  void _isr14();
extern  void _isr15();
extern  void _isr16();
extern  void _isr17();
extern  void _isr18();
extern  void _isr19();
extern  void _isr20();
extern  void _isr21();
extern  void _isr22();
extern  void _isr23();
extern  void _isr24();
extern  void _isr25();
extern  void _isr26();
extern  void _isr27();
extern  void _isr28();
extern  void _isr29();
extern  void _isr30();
extern  void _isr31();

extern  void irq0();
extern  void irq1();
extern  void irq2();
extern  void irq3();
extern  void irq4();
extern  void irq5();
extern  void irq6();
extern  void irq7();
extern  void irq8();
extern  void irq9();
extern  void irq10();
extern  void irq11();
extern  void irq12();
extern  void irq13();
extern  void irq14();
extern  void irq15();

void init_interupts() {

    populate_gate(0, _isr0, DEFAULT_SELECTOR, 0, INTERUPT_GATE); 
    populate_gate(1, _isr1, DEFAULT_SELECTOR, 0, INTERUPT_GATE); 
    populate_gate(2, _isr2, DEFAULT_SELECTOR, 0, INTERUPT_GATE); 
    populate_gate(3, _isr3, DEFAULT_SELECTOR, 0, INTERUPT_GATE); 
    populate_gate(4, _isr4, DEFAULT_SELECTOR, 0, INTERUPT_GATE); 
    populate_gate(5, _isr5, DEFAULT_SELECTOR, 0, INTERUPT_GATE); 
    populate_gate(6, _isr6, DEFAULT_SELECTOR, 0, INTERUPT_GATE); 
    populate_gate(7, _isr7, DEFAULT_SELECTOR, 0, INTERUPT_GATE); 
    populate_gate(8, _isr8, DEFAULT_SELECTOR, 0, INTERUPT_GATE); 
    populate_gate(9, _isr9, DEFAULT_SELECTOR, 0, INTERUPT_GATE); 
    populate_gate(10, _isr10, DEFAULT_SELECTOR, 0, INTERUPT_GATE); 
    populate_gate(11, _isr11, DEFAULT_SELECTOR, 0, INTERUPT_GATE); 
    populate_gate(12, _isr12, DEFAULT_SELECTOR, 0, INTERUPT_GATE); 
    populate_gate(13, _isr13, DEFAULT_SELECTOR, 0, INTERUPT_GATE); 
    populate_gate(14, _isr14, DEFAULT_SELECTOR, 0, INTERUPT_GATE); 
    populate_gate(15, _isr15, DEFAULT_SELECTOR, 0, INTERUPT_GATE); 
    populate_gate(16, _isr16, DEFAULT_SELECTOR, 0, INTERUPT_GATE); 
    populate_gate(17, _isr17, DEFAULT_SELECTOR, 0, INTERUPT_GATE); 
    populate_gate(18, _isr18, DEFAULT_SELECTOR, 0, INTERUPT_GATE); 
    populate_gate(19, _isr19, DEFAULT_SELECTOR, 0, INTERUPT_GATE); 
    populate_gate(20, _isr20, DEFAULT_SELECTOR, 0, INTERUPT_GATE); 
    populate_gate(21, _isr21, DEFAULT_SELECTOR, 0, INTERUPT_GATE); 
    populate_gate(22, _isr22, DEFAULT_SELECTOR, 0, INTERUPT_GATE); 
    populate_gate(23, _isr23, DEFAULT_SELECTOR, 0, INTERUPT_GATE); 
    populate_gate(24, _isr24, DEFAULT_SELECTOR, 0, INTERUPT_GATE); 
    populate_gate(25, _isr25, DEFAULT_SELECTOR, 0, INTERUPT_GATE); 
    populate_gate(26, _isr26, DEFAULT_SELECTOR, 0, INTERUPT_GATE); 
    populate_gate(27, _isr27, DEFAULT_SELECTOR, 0, INTERUPT_GATE); 
    populate_gate(28, _isr28, DEFAULT_SELECTOR, 0, INTERUPT_GATE); 
    populate_gate(29, _isr29, DEFAULT_SELECTOR, 0, INTERUPT_GATE); 
    populate_gate(30, _isr30, DEFAULT_SELECTOR, 0, INTERUPT_GATE); 
    populate_gate(31, _isr31, DEFAULT_SELECTOR, 0, INTERUPT_GATE); 

    init_pic();

    populate_gate(32, irq0, DEFAULT_SELECTOR, 0, INTERUPT_GATE);
    populate_gate(33, irq1, DEFAULT_SELECTOR, 0, INTERUPT_GATE);
    populate_gate(34, irq2, DEFAULT_SELECTOR, 0, INTERUPT_GATE);
    populate_gate(35, irq3, DEFAULT_SELECTOR, 0, INTERUPT_GATE);
    populate_gate(36, irq4, DEFAULT_SELECTOR, 0, INTERUPT_GATE);
    populate_gate(37, irq5, DEFAULT_SELECTOR, 0, INTERUPT_GATE);
    populate_gate(38, irq6, DEFAULT_SELECTOR, 0, INTERUPT_GATE);
    populate_gate(39, irq7, DEFAULT_SELECTOR, 0, INTERUPT_GATE);
    populate_gate(40, irq8, DEFAULT_SELECTOR, 0, INTERUPT_GATE);
    populate_gate(41, irq9, DEFAULT_SELECTOR, 0, INTERUPT_GATE);
    populate_gate(42, irq10, DEFAULT_SELECTOR, 0, INTERUPT_GATE);
    populate_gate(43, irq11, DEFAULT_SELECTOR, 0, INTERUPT_GATE);
    populate_gate(44, irq12, DEFAULT_SELECTOR, 0, INTERUPT_GATE);
    populate_gate(45, irq13, DEFAULT_SELECTOR, 0, INTERUPT_GATE);
    populate_gate(46, irq14, DEFAULT_SELECTOR, 0, INTERUPT_GATE);
    populate_gate(47, irq15, DEFAULT_SELECTOR, 0, INTERUPT_GATE);
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
static void _int_handler(struct registers *regs, int num) {
    irq_specific_handler_t handler = handler_entries[num - 32];

    if (regs) {}
    if (!handler) {
        // ack and return
        println("no handler");
        pic_eoi(num);
        return;
    }

    // TODO

    //if (handler(regs)) {
    //    return;
    //}
    pic_eoi(num);
}

// main entrypoint for the interupts (from the asm)
void interupt_handler (struct registers* regs) {
    // TODO: call _int_handler
    println(to_string(regs->int_no));
    println(to_string(regs->err_code));
    println(to_string(regs->rsp));
    _int_handler(regs, regs->int_no);
    return;
}

void disable_interupts() {
    asm volatile ("cli");
}

void enable_interupts() {
    asm volatile ("sti");
}
