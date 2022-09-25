#include "idt.h"
#include "arch/x86/interupts/interupts.h"
#include <arch/x86/dev/debug/serial.h>
#include <libc/stddef.h>
#include <arch/x86/mem/kmem_manager.h>
#include <libc/string.h>

#define INTERRUPT_VECTORS_BASE 0xFFFFffffFF000000
idt_entry_t idt_entries[MAX_IDT_ENTRIES];

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
    idt_ptr.limit = 0x0FFF;
    idt_ptr.base = (uintptr_t)idt_entries;

    init_interupts();

    // TODO: mmap idt base?

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
