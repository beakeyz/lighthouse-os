# ifndef __IDT__
# define __IDT__

#include <libk/stddef.h>
#include "interrupts.h"

// Some defines that are idt/irq/isr specific
#define MAX_IDT_ENTRIES     256
#define DEFAULT_SELECTOR    0x08
#define INTERRUPT_GATE       0x8E
// FIXME: this value correct?
#define TRAP_GATE           0x8F

// TODO
typedef struct {
    uint16_t    limit;
    uintptr_t   base;
} __attribute__((packed)) idt_ptr_t;

// TODO
typedef struct {          // + 0 bytes
    uint16_t base_low;    // + 2 bytes
    uint16_t selector;    // + 4 bytes

    uint8_t ist;          // + 5 bytes
    uint8_t flags;        // + 6 bytes

    uint16_t base_mid;    // + 8 bytes
    uint32_t base_high;   // + 12 bytes
    uint32_t pad;         // + 16 bytes
} __attribute__((packed)) idt_entry_t;


// add shit
idt_entry_t get_idt (int idx);

// install shit
void setup_idt (bool should_zero);

void register_idt_interrupt_handler(uint16_t num, void (*handler)());
void register_idt_trap_handler(uint16_t num, void (*handler)());

void idt_set_gate(uint16_t num, uint8_t flags, uint16_t selector, void(*handler)());

void flush_idt ();

#endif
