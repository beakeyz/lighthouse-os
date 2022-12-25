#include "idt.h"
#include "kernel/interupts/interupts.h"
#include "kernel/mem/kmalloc.h"
#include <kernel/dev/debug/serial.h>
#include <kernel/mem/kmem_manager.h>
#include <libk/stddef.h>
#include <libk/string.h>

idt_entry_t idt_entries[MAX_IDT_ENTRIES];

void idt_set_gate(uint16_t num, uint8_t flags, uint16_t selector,
                  interrupt_callback_t handler) {
  idt_entries[num].base_low = (uint16_t)((uint64_t)handler & 0xFFFF);
  idt_entries[num].base_mid = (uint16_t)((uint64_t)handler >> 16);
  idt_entries[num].base_high = (uint32_t)((uint64_t)handler >> 32);
  idt_entries[num].selector = selector;
  idt_entries[num].ist = 0;
  idt_entries[num].pad = 0;
  idt_entries[num].flags = flags;
}

void setup_idt() {
  idt_ptr_t idtr;

  memset(idt_entries, 0, sizeof(idt_entry_t) * MAX_IDT_ENTRIES);
  memset(&idtr, 0, sizeof(idtr));

  idtr.limit = sizeof(idt_entry_t) * MAX_IDT_ENTRIES - 1;
  idtr.base = (uintptr_t)&idt_entries;


  asm volatile("lidt %0" : : "g"(idtr));
}

idt_entry_t get_idt(int idx) { return idt_entries[idx]; }

