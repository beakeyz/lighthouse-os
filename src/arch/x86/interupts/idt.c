#include "idt.h"
#include <arch/x86/dev/debug/serial.h>
#include <libc/stddef.h>

idt_ptr_t idt_ptr;
idt_entry_t idt_entries[MAX_IDT_ENTRIES];


static irq_handler_t _handlers[128] = {
    _isr0,
    _isr1,
    _isr2,
    _isr3,
    _isr4,
    _isr5,
    _isr6,
    _isr7,
    _isr8,
    _isr9,
    _isr10,
    _isr11,
    _isr12,
    _isr13,
    _isr14,
    _isr15,
    _isr16,
    _isr17,
    _isr18,
    _isr19,
    _isr20,
    _isr21,
    _isr22,
    _isr23,
    _isr24,
    _isr25,
    _isr26,
    _isr27,
    _isr28,
    _isr29,
    _isr30,
    _isr31,
    _isr32,
    _isr33,
    _isr34,
    _isr35,
    _isr36,
    _isr37,
    _isr38,
    _isr39,
    _isr40,
    _isr41,
    _isr42,
    _isr43,
    _isr44,
    _isr45,
    _isr46,
    _isr47,
};


void populate_gate(uint8_t num, void* handler, uint16_t selector, uint8_t ist, uint8_t flags) {
    uintptr_t base = (uintptr_t) handler;
    uint16_t base_low = (uint16_t) (base);
    uint16_t base_mid = (uint16_t) ((base >> 16));
    uint32_t base_high = (uint32_t) ((base >> 32));

    // padding (yuck)
    idt_entries[num].pad = 0;

    // cool stuff
    idt_entries[num].selector = selector;
    idt_entries[num].ist = ist;
    idt_entries[num].flags = flags;

    // bases
    idt_entries[num].base_low = base_low;
    idt_entries[num].base_mid = base_mid;
    idt_entries[num].base_high = base_high;
}

void setup_idt() {

    println("setup idt");
    // Store addr and size of the idt table in the pointer
    idt_ptr.limit = sizeof(idt_entry_t) * MAX_IDT_ENTRIES;
    idt_ptr.base = (uintptr_t)&idt_entries[0];

    // loop over all the 47 isrs
    for (int i = 0; i < 48; i++) {
        if (i == 14  || i == 32) {
            populate_gate(i, (void*)_handlers[i], DEFAULT_SELECTOR, 1, INTERUPT_GATE);
            continue;
        }
        populate_gate(i, (void*)_handlers[i], DEFAULT_SELECTOR, 0, INTERUPT_GATE);
    }
    // Load the idt
    flush_idt((uintptr_t)&idt_ptr);
}

void handle_isr(struct registers *regs) {
    if (regs) {}
}

void handle_irq(struct registers *regs) {
    if (regs) {}
}
