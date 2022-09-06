# ifndef __IDT__
# define __IDT__

#include <libc/stddef.h>
#include "interupts.h"

// Some defines that are idt/irq/isr specific
#define MAX_IDT_ENTRIES     256
#define DEFAULT_SELECTOR    0x08
#define INTERUPT_GATE       0x8E
#define TRAP_GATE           0x8F

// TODO
typedef struct {
    uint16_t    limit;
    uintptr_t   base;
} __attribute__((__packed__)) idt_ptr_t;

// TODO
typedef struct {
    uint16_t base_low;
    uint16_t selector;

    uint8_t zero;
    uint8_t flags;

    uint16_t base_mid;
    uint32_t base_high;
    uint32_t pad;
} __attribute__((__packed__)) idt_entry_t;


// add shit
void populate_gate(uint8_t num, irq_handler_t handler, uint16_t selector, uint8_t flags);

// install shit
void setup_idt ();

// handle shit
void handle_isr (struct registers* regs);

// handle more shit
void handle_irq (struct registers* regs);

#endif
