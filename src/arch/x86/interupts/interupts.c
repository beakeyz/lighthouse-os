#include "interupts.h"
#include "arch/x86/interupts/idt.h"
#include "libk/string.h"
#include <arch/x86/interupts/control/pic.h>
#include <arch/x86/dev/debug/serial.h>
#include <libk/stddef.h>

static irq_specific_handler_t handler_entries[16] = { NULL };

extern void interrupt_service_routine_0();
extern void interrupt_service_routine_1();
extern void interrupt_service_routine_2();
extern void interrupt_service_routine_3();
extern void interrupt_service_routine_4();
extern void interrupt_service_routine_5();
extern void interrupt_service_routine_6();
extern void interrupt_service_routine_7();
extern void interrupt_service_routine_error_code_8();
extern void interrupt_service_routine_9();
extern void interrupt_service_routine_error_code_10();
extern void interrupt_service_routine_error_code_11();
extern void interrupt_service_routine_error_code_12();
extern void interrupt_service_routine_error_code_13();
extern void interrupt_service_routine_error_code_14();
extern void interrupt_service_routine_15();
extern void interrupt_service_routine_16();
extern void interrupt_service_routine_error_code_17();
extern void interrupt_service_routine_18();
extern void interrupt_service_routine_32();
extern void interrupt_service_routine_33();
extern void interrupt_service_routine_34();
extern void interrupt_service_routine_35();
extern void interrupt_service_routine_36();
extern void interrupt_service_routine_37();
extern void interrupt_service_routine_38();
extern void interrupt_service_routine_39();
extern void interrupt_service_routine_40();
extern void interrupt_service_routine_41();
extern void interrupt_service_routine_42();
extern void interrupt_service_routine_43();
extern void interrupt_service_routine_44();
extern void interrupt_service_routine_45();
extern void interrupt_service_routine_46();
extern void interrupt_service_routine_47();
extern void interrupt_service_routine_255();

void init_interupts() {

    idt_set_gate(0x00, 0x8E, 0x8, interrupt_service_routine_0);
    idt_set_gate(0x01, 0x8E, 0x8, interrupt_service_routine_1);
    idt_set_gate(0x02, 0x8E, 0x8, interrupt_service_routine_2);
    idt_set_gate(0x03, 0x8E, 0x8, interrupt_service_routine_3);
    idt_set_gate(0x04, 0x8E, 0x8, interrupt_service_routine_4);
    idt_set_gate(0x05, 0x8E, 0x8, interrupt_service_routine_5);
    idt_set_gate(0x06, 0x8E, 0x8, interrupt_service_routine_6);
    idt_set_gate(0x07, 0x8E, 0x8, interrupt_service_routine_7);
    idt_set_gate(0x08, 0x8E, 0x8, interrupt_service_routine_error_code_8);
    idt_set_gate(0x09, 0x8E, 0x8, interrupt_service_routine_9);
    idt_set_gate(0x0A, 0x8E, 0x8, interrupt_service_routine_error_code_10);
    idt_set_gate(0x0B, 0x8E, 0x8, interrupt_service_routine_error_code_11);
    idt_set_gate(0x0C, 0x8E, 0x8, interrupt_service_routine_error_code_12);
    idt_set_gate(0x0D, 0x8E, 0x8, interrupt_service_routine_error_code_13);
    idt_set_gate(0x0E, 0x8E, 0x8, interrupt_service_routine_error_code_14);
    idt_set_gate(0x0F, 0x8E, 0x8, interrupt_service_routine_15);
    idt_set_gate(0x10, 0x8E, 0x8, interrupt_service_routine_16);
    idt_set_gate(0x11, 0x8E, 0x8, interrupt_service_routine_error_code_17);
    idt_set_gate(0x12, 0x8E, 0x8, interrupt_service_routine_18);
    idt_set_gate(0x20, 0x8E, 0x8, interrupt_service_routine_32);
    idt_set_gate(0x21, 0x8E, 0x8, interrupt_service_routine_33);
    idt_set_gate(0x22, 0x8E, 0x8, interrupt_service_routine_34);
    idt_set_gate(0x23, 0x8E, 0x8, interrupt_service_routine_35);
    idt_set_gate(0x24, 0x8E, 0x8, interrupt_service_routine_36);
    idt_set_gate(0x25, 0x8E, 0x8, interrupt_service_routine_37);
    idt_set_gate(0x26, 0x8E, 0x8, interrupt_service_routine_38);
    idt_set_gate(0x27, 0x8E, 0x8, interrupt_service_routine_39);
    idt_set_gate(0x28, 0x8E, 0x8, interrupt_service_routine_40);
    idt_set_gate(0x29, 0x8E, 0x8, interrupt_service_routine_41);
    idt_set_gate(0x2a, 0x8E, 0x8, interrupt_service_routine_42);
    idt_set_gate(0x2b, 0x8E, 0x8, interrupt_service_routine_43);
    idt_set_gate(0x2c, 0x8E, 0x8, interrupt_service_routine_44);
    idt_set_gate(0x2d, 0x8E, 0x8, interrupt_service_routine_45);
    idt_set_gate(0x2e, 0x8E, 0x8, interrupt_service_routine_46);
    idt_set_gate(0x2f, 0x8E, 0x8, interrupt_service_routine_47);
    idt_set_gate(0xFF, 0x8E, 0x8, interrupt_service_routine_255);

    init_pic();
}

// FIXME: look into the idea of having a error datastruct for this type of stuff...
// otherwise return an int with statuscode?
void add_handler(size_t irq_num, irq_specific_handler_t handler_ptr) {
    disable_interupts();
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
    disable_interupts();
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
registers_t* interrupt_handler (struct registers* regs) {
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
