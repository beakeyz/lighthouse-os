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

    simple_populate(0, _isr0, 0); 
    simple_populate(1, _isr1, 0); 
    simple_populate(2, _isr2, 0); 
    simple_populate(3, _isr3, 0); 
    simple_populate(4, _isr4, 0); 
    simple_populate(5, _isr5, 0); 
    simple_populate(6, _isr6, 0); 
    simple_populate(7, _isr7, 0); 
    simple_populate(8, _isr8, 0); 
    simple_populate(9, _isr9, 0); 
    simple_populate(10, _isr10, 0); 
    simple_populate(11, _isr11, 0); 
    simple_populate(12, _isr12, 0); 
    simple_populate(13, _isr13, 0); 
    simple_populate(14, _isr14, 0); 
    simple_populate(15, _isr15, 0); 
    simple_populate(16, _isr16, 0); 
    simple_populate(17, _isr17, 0); 
    simple_populate(18, _isr18, 0); 
    simple_populate(19, _isr19, 0); 
    simple_populate(20, _isr20, 0); 
    simple_populate(21, _isr21, 0); 
    simple_populate(22, _isr22, 0); 
    simple_populate(23, _isr23, 0); 
    simple_populate(24, _isr24, 0); 
    simple_populate(25, _isr25, 0); 
    simple_populate(26, _isr26, 0); 
    simple_populate(27, _isr27, 0); 
    simple_populate(28, _isr28, 0); 
    simple_populate(29, _isr29, 0); 
    simple_populate(30, _isr30, 0); 
    simple_populate(31, _isr31, 0); 

    init_pic();

    simple_populate(32, irq0, 0);
    simple_populate(33, irq1, 0);
    simple_populate(34, irq2, 0);
    simple_populate(35, irq3, 0);
    simple_populate(36, irq4, 0);
    simple_populate(37, irq5, 0);
    simple_populate(38, irq6, 0);
    simple_populate(39, irq7, 0);
    simple_populate(40, irq8, 0);
    simple_populate(41, irq9, 0);
    simple_populate(42, irq10, 0);
    simple_populate(43, irq11, 0);
    simple_populate(44, irq12, 0);
    simple_populate(45, irq13, 0);
    simple_populate(46, irq14, 0);
    simple_populate(47, irq15, 0);
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
