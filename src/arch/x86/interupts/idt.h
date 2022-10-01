# ifndef __IDT__
# define __IDT__

#include <libc/stddef.h>
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
typedef struct {
    uint16_t base_low;
    uint16_t selector;

    uint8_t ist;
    uint8_t flags;

    uint16_t base_mid;
    uint32_t base_high;
    uint32_t pad;
} __attribute__((packed)) idt_entry_t;


// add shit

// install shit
void setup_idt ();

// handle shit
void handle_isr (struct registers* regs);

// handle more shit
void handle_irq (struct registers* regs);

void idt_set_gate(uint16_t num, uint8_t flags, uint16_t selector, void (*handler)() );

extern void flush_idt (uintptr_t ptr);

#endif
