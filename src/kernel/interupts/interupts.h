#ifndef __INTERUPTS__
#define __INTERUPTS__

#include "interupts/control/interrupt_control.h"
#include "system/processor/registers.h"
#include <libk/stddef.h>

// NOTE: this file CAN NOT include idt.h, because of an include loop =/

struct Interrupt;
struct InterruptHandler;

// handler func ptrs
typedef registers_t* (*interrupt_callback_t)(registers_t*);
typedef int (*irq_handler_t) (registers_t*);

// Represent the interrupt when fired
typedef struct Interrupt {
  irq_handler_t m_handler;
  uint32_t m_irq_num;
  INTERRUPT_CONTROLLER_TYPE m_type;
} Interrupt_t;

// Represent the interrupthandler that should handle a fired Interrupt_t
typedef struct InterruptHandler {
  Interrupt_t* m_interrupt;
  InterruptController_t* m_controller;
} InterruptHandler_t;

// init 0.0
void init_interupts();

// add
void add_handler (size_t irq_num, irq_handler_t handler_ptr);

// remove
void remove_handler (size_t irq_num);

// main entrypoint
registers_t* interrupt_handler (struct registers* regs);

// ack (this should prob be specific to the chip we use lol)
// extern void interupt_acknowledge (int num);

void disable_interupts ();
void enable_interupts ();

#endif // !__INTERUPTS__

