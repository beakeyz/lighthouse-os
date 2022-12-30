#ifndef __INTERUPTS__
#define __INTERUPTS__

#include "interupts/control/interrupt_control.h"
#include "system/processor/registers.h"
#include <libk/stddef.h>

// NOTE: this file CAN NOT include idt.h, because of an include loop =/

struct Interrupt;
struct InterruptHandler;

// handler func ptrs
typedef void (*interrupt_exeption_t) ();
typedef void (*interrupt_error_handler_t)(registers_t*);
typedef registers_t* (*interrupt_callback_t)(registers_t*);

// Represent the interrupthandler that should handle a fired Interrupt_t
typedef struct InterruptHandler {
  interrupt_callback_t fHandler;
  uint16_t m_int_num;
  bool m_is_registerd;
  bool m_is_in_interrupt; // TODO: atomic?
  InterruptController_t* m_controller;
} InterruptHandler_t;

InterruptHandler_t* init_interrupt_handler(uint16_t int_num, INTERRUPT_CONTROLLER_TYPE type, interrupt_callback_t callback);
InterruptHandler_t init_unhandled_interrupt_handler(uint16_t int_num);

// init 0.0
void init_interupts();

// add
bool add_handler (InterruptHandler_t* handler_ptr);

// remove
void remove_handler (const uint16_t int_num);

// main entrypoint
registers_t* interrupt_handler (struct registers* regs);

// ack (this should prob be specific to the chip we use lol)
// extern void interupt_acknowledge (int num);

void disable_interupts ();
void enable_interupts ();

#endif // !__INTERUPTS__

