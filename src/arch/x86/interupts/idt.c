#include "idt.h"
#include "arch/x86/interupts/interupts.h"
#include "arch/x86/mem/kmalloc.h"
#include <arch/x86/dev/debug/serial.h>
#include <libc/stddef.h>
#include <arch/x86/mem/kmem_manager.h>
#include <libc/string.h>

static idt_ptr_t idtr;
static idt_entry_t idt_entries[MAX_IDT_ENTRIES];

void idt_set_gate(uint16_t num, uint8_t flags, uint16_t selector, void (*handler)() ) {
    idt_entries[num].base_low = (uint16_t) ((uint64_t)handler&0xFFFF);
    idt_entries[num].base_mid = (uint16_t) ((uint64_t)handler >> 16);
    idt_entries[num].base_high = (uint32_t)((uint64_t)handler>> 32);
	idt_entries[num].selector = selector;
	idt_entries[num].ist = 0;
	idt_entries[num].pad = 0;
	idt_entries[num].flags = flags;
}

void setup_idt() {

    println("setup idt");
    // Store addr and size of the idt table in the pointer
    idtr.limit = sizeof(idt_entry_t) * MAX_IDT_ENTRIES - 1;
    idtr.base = (uintptr_t)&idt_entries;

    int i = 0;
    while (i < MAX_IDT_ENTRIES){
        idt_entries[i].flags = 0;
        idt_entries[i].ist = 0;
        idt_entries[i].base_high = 0;
        idt_entries[i].base_low = 0;
        idt_entries[i].pad = 0;
        idt_entries[i].base_mid = 0;
        idt_entries[i].selector = 0;
        i++;
    }

    init_interupts();

    // TODO: mmap idt base?

    // Load the idt
    //load_standard_idtptr();
    asm volatile("lidt %0" : : "g"(idtr));
}

void handle_isr(struct registers *regs) {
    if (regs) {}
}

void handle_irq(struct registers *regs) {
    if (regs) {}
}
