#include "idt.h"
#include <arch/x86/dev/debug/serial.h>
#include <libc/stddef.h>
#include <libc/string.h>

idt_ptr_t idt_ptr;
idt_entry_t idt_entries[MAX_IDT_ENTRIES];


void populate_gate(uint8_t num, void* handler, uint16_t selector, uint8_t ist, uint8_t flags) {
    uintptr_t base = (uintptr_t) handler;
    uint16_t base_low = (uint16_t) (base & 0xFFFF);
    uint16_t base_mid = (uint16_t) ((base >> 16) & 0xFFFF);
    uint32_t base_high = (uint32_t) ((base >> 32) & 0xFFFF);

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
    idt_ptr.limit = (sizeof(idt_entry_t) * MAX_IDT_ENTRIES) - 1;
    idt_ptr.base = (uintptr_t)&idt_entries;

    memset(&idt_entries, 0, sizeof(idt_entry_t) * 256);

    // Load the idt
    load_standard_idtptr();
}

void handle_isr(struct registers *regs) {
    if (regs) {}
}

void handle_irq(struct registers *regs) {
    if (regs) {}
}
