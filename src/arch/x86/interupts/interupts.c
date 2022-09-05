#include "interupts.h"
#include <arch/x86/dev/debug/serial.h>
#include <libc/stddef.h>

static irq_specific_handler_t handler_entries[16] = { NULL };

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
    irq_specific_handler_t handler = handler_entries[num];

    if (!handler) {
        // ack and return
        interupt_acknowledge(num);
        return;
    }

    // TODO
    if (handler(regs)) {
        return;
    }
}

// main entrypoint for the interupts (from the asm)
struct registers* interupt_handler (struct registers* regs) {
    // TODO: call _int_handler
    println("cool =D");
    if (regs && regs->int_no >= 0 && regs->int_no <= 127) {
        _int_handler(regs,  regs->int_no);
    }

    return regs;
} 

void interupt_acknowledge(int num) {
    if (num >= 8) {
    //	out8(0xA0, 0x20);
	}
	//out8(0x20, 0x20);
}
