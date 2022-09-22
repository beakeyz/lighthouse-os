#include "idt.h"
#include <arch/x86/dev/debug/serial.h>
#include <libc/stddef.h>
#include <arch/x86/mem/kmem_manager.h>
#include <libc/string.h>

idt_entry_t idt_entries[MAX_IDT_ENTRIES];


void populate_gate(uint8_t num, void* handler, uint16_t selector, uint8_t ist, uint8_t flags) {
    uintptr_t base = (uintptr_t) handler;
    uint16_t base_low = (uint16_t) (base & 0xFFFF);
    uint16_t base_mid = (uint16_t) ((base >> 16) & 0xFFFF);
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

void simple_populate(uint8_t num, void *handler, uint8_t dpl) {
    uint64_t handler_addr = (uint64_t)handler;

    idt_entry_t* entry = &idt_entries[num];
    entry->base_low = handler_addr & 0xFFFF;
    entry->base_mid = (handler_addr >> 16) & 0xFFFF;
    entry->base_high = handler_addr >> 32;
    //your code selector may be different!
    entry->selector = DEFAULT_SELECTOR;
    //trap gate + present + DPL
    entry->flags = 0b1110 | ((dpl & 0b11) << 5) | (1 << 7);
    //ist disabled
    entry->ist = 0;
}

void setup_idt() {

    println("setup idt");
    idt_ptr_t idt_ptr;
    // Store addr and size of the idt table in the pointer
    idt_ptr.limit = MAX_IDT_ENTRIES * sizeof(idt_entry_t) - 1;
    idt_ptr.base = (uintptr_t)&idt_entries;

    // I am going to leave this in for now =)
    print("Physical addr of the idt table: ");
    println(to_string((uintptr_t)&idt_entries));
    print("Virtual addr of the idt table: ");
    println(to_string((uintptr_t)kmem_from_phys((uintptr_t)&idt_entries)));
    print("Physical addr of the idt table, but retranslated: ");
    println(to_string((uintptr_t)kmem_to_phys(nullptr, (uintptr_t)kmem_from_phys((uintptr_t)&idt_entries))));

    // Load the idt
    //load_standard_idtptr();
    asm volatile("lidt %0" :: "m"(idt_ptr));
}

void handle_isr(struct registers *regs) {
    if (regs) {}
}

void handle_irq(struct registers *regs) {
    if (regs) {}
}
