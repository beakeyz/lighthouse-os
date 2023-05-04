#include "idt.h"
#include "interupts/interupts.h"
#include <mem/heap.h>
#include <dev/debug/serial.h>
#include <mem/kmem_manager.h>
#include <libk/stddef.h>
#include <libk/string.h>

static idt_entry_t idt_entries[MAX_IDT_ENTRIES];
static idt_ptr_t idtr;

static idt_entry_t create_idt_entry(void (*handler)(), uint16_t selector, uint8_t type);

// TODO: shit

static idt_entry_t create_idt_entry(void (*handler)(), uint16_t selector, uint8_t type) {

  idt_entry_t ret = {
    .base_low = (uint16_t)((uintptr_t)handler & 0xffff),
    .base_mid = (uint16_t)((uintptr_t)handler >> 16) & 0xFFFF,
    .base_high = (uint32_t)((uintptr_t)handler >> 32) & 0xFFFFFFFF,
    .selector = selector,
    .ist = 0,
    .pad = 0,
    .flags = type,
  };

  return ret;
}

void idt_set_gate(uint16_t num, uint8_t flags, uint16_t selector, void(*handler)()) {

  idt_entries[num].base_low = (uint16_t)((uint64_t)handler & 0xFFFF);
  idt_entries[num].base_mid = (uint16_t)((uint64_t)handler >> 16) & 0xFFFF;
  idt_entries[num].base_high = (uint32_t)((uint64_t)handler >> 32) & 0xFFFFFFFF;
  idt_entries[num].selector = selector;
  idt_entries[num].ist = 0;
  idt_entries[num].pad = 0;
  idt_entries[num].flags = flags;
}

void register_idt_interrupt_handler(uint16_t num, void (*handler)()) {
  idt_entry_t entry = create_idt_entry(handler, DEFAULT_SELECTOR, INTERRUPT_GATE);
  idt_entries[num] = entry;
}
void register_idt_trap_handler(uint16_t num, void (*handler)()) {
  idt_entry_t entry = create_idt_entry(handler, DEFAULT_SELECTOR, TRAP_GATE);
}

void setup_idt(bool should_zero) {

  if (should_zero) {
    memset(idt_entries, 0, sizeof(idt_entry_t) * MAX_IDT_ENTRIES);
    memset(&idtr, 0, sizeof(idtr));
  }

  idtr.limit = sizeof(idt_entry_t) * MAX_IDT_ENTRIES - 1;
  idtr.base = (uintptr_t)&idt_entries;

}

void flush_idt () {
  asm volatile("lidt %0" : : "g"(idtr));
}

idt_entry_t get_idt(int idx) { return idt_entries[idx]; }

