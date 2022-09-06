#include "idt.h"
#include <libc/stddef.h>
#include <arch/x86/dev/debug/serial.h>

idt_entry_t idt_entries[MAX_IDT_ENTRIES];

void populate_gate(uint8_t num, irq_handler_t handler, uint16_t selector, uint8_t flags) {
    uintptr_t base = (uintptr_t) handler;
    uint16_t base_low = (uint16_t) (base & 0xFFFF);
    uint16_t base_mid = (uint16_t) ((base >> 16)&0xFFFF);
    uint32_t base_high = (uint32_t) ((base >> 32)&0xFFFFffff);

    idt_entries[num].base_low = base_low;
    idt_entries[num].base_mid = base_mid;
    idt_entries[num].base_high = base_high;
    idt_entries[num].selector = selector;
    idt_entries[num].zero = 0;
    idt_entries[num].pad = 0;
    idt_entries[num].flags = flags;
}

void setup_idt() {
    idt_ptr_t idt_ptr;
    // Store addr and size of the idt table in the pointer
    idt_ptr.limit = MAX_IDT_ENTRIES * sizeof(idt_entry_t) - 1;
    idt_ptr.base = (uintptr_t)&idt_entries;

    populate_gate(32, _irq32, DEFAULT_SELECTOR, INTERUPT_GATE); 
    populate_gate(33, _irq33, DEFAULT_SELECTOR, INTERUPT_GATE); 
    populate_gate(34, _irq34, DEFAULT_SELECTOR, INTERUPT_GATE); 
    populate_gate(35, _irq35, DEFAULT_SELECTOR, INTERUPT_GATE); 
    populate_gate(36, _irq36, DEFAULT_SELECTOR, INTERUPT_GATE); 
    populate_gate(37, _irq37, DEFAULT_SELECTOR, INTERUPT_GATE); 
    populate_gate(38, _irq38, DEFAULT_SELECTOR, INTERUPT_GATE); 
    populate_gate(39, _irq39, DEFAULT_SELECTOR, INTERUPT_GATE); 
    populate_gate(40, _irq40, DEFAULT_SELECTOR, INTERUPT_GATE); 
    populate_gate(41, _irq41, DEFAULT_SELECTOR, INTERUPT_GATE); 
    populate_gate(42, _irq42, DEFAULT_SELECTOR, INTERUPT_GATE); 
    populate_gate(43, _irq43, DEFAULT_SELECTOR, INTERUPT_GATE); 
    populate_gate(44, _irq44, DEFAULT_SELECTOR, INTERUPT_GATE); 
    populate_gate(45, _irq45, DEFAULT_SELECTOR, INTERUPT_GATE); 
    populate_gate(46, _irq46, DEFAULT_SELECTOR, INTERUPT_GATE); 
    populate_gate(47, _irq47, DEFAULT_SELECTOR, INTERUPT_GATE); 

    // Load the idt
    asm volatile (
        "lidt %0" :: "g"(idt_ptr)
    );
}

void handle_isr(struct registers *regs) {

}

void handle_irq(struct registers *regs) {

}
