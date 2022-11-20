# ifndef __IDT__
# define __IDT__

#include <libk/stddef.h>
#include "interupts.h"

// Some defines that are idt/irq/isr specific
#define MAX_IDT_ENTRIES     256
#define DEFAULT_SELECTOR    0x08
#define INTERUPT_GATE       0x8E
// FIXME: this value correct?
#define TRAP_GATE           0x8F


typedef registers_t * (*interrupt_handler_t)(registers_t*);
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
void setup_idt ();

// handle shit
void handle_isr (struct registers* regs);

// handle more shit
void handle_irq (struct registers* regs);

void idt_set_gate(uint16_t num, uint8_t flags, uint16_t selector, registers_t* (*handler)(registers_t*) );

extern void flush_idt (uintptr_t ptr);

#endif
