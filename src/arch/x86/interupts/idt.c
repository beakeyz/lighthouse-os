#include "idt.h"
#include "arch/x86/interupts/interupts.h"
#include <arch/x86/dev/debug/serial.h>
#include <libc/stddef.h>
#include <arch/x86/mem/kmem_manager.h>
#include <libc/string.h>

static idt_ptr_t idtp;
static idt_entry_t idt_entries[MAX_IDT_ENTRIES];

void idt_set_gate(uint8_t num, interrupt_handler_t handler, uint16_t selector, uint8_t flags, int userspace) {
	uintptr_t base = (uintptr_t)handler;
	idt_entries[num].base_low  = (base & 0xFFFF);
	idt_entries[num].base_mid  = (base >> 16) & 0xFFFF;
	idt_entries[num].base_high = (base >> 32) & 0xFFFFFFFF;
	idt_entries[num].selector = selector;
	idt_entries[num].ist = 0;
	idt_entries[num].pad = 0;
	idt_entries[num].flags = flags | (userspace ? 0x60 : 0);
}

void setup_idt() {

    println("setup idt");
    // Store addr and size of the idt table in the pointer
    idtp.limit = sizeof(idt_entries);
    idtp.base = (uintptr_t)&idt_entries;

    init_interupts();

    // TODO: mmap idt base?

    // Load the idt
    //load_standard_idtptr();
    asm volatile("lidt %0" : : "m"(idtp));
}

void handle_isr(struct registers *regs) {
    if (regs) {}
}

void handle_irq(struct registers *regs) {
    if (regs) {}
}
