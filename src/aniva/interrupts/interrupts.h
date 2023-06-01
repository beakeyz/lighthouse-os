#ifndef __ANIVA_INTERRUPTS__
#define __ANIVA_INTERRUPTS__

#include "interrupts/control/interrupt_control.h"
#include "system/processor/registers.h"
#include <libk/stddef.h>

// NOTE: this file CAN NOT include idt.h, because of an include loop =/

#define IRQ_VEC_BASE 32
#define INTERRUPT_HANDLER_COUNT (256 - IRQ_VEC_BASE)

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

InterruptHandler_t* create_interrupt_handler(uint16_t int_num, INTERRUPT_CONTROLLER_TYPE type, interrupt_callback_t callback);
InterruptHandler_t create_unhandled_interrupt_handler(uint16_t int_num);

// init 0.0
void init_interrupts();

// add
bool interrupts_add_handler (InterruptHandler_t* handler_ptr);

// remove
void interrupts_remove_handler (const uint16_t int_num);

// main entrypoint
registers_t* interrupt_handler (struct registers* regs);

// ack (this should prob be specific to the chip we use lol)
// extern void interupt_acknowledge (int num);

void disable_interrupts();
void enable_interrupts ();

// hopefully this does not get used anywhere else ;-;
#define CHECK_AND_DO_DISABLE_INTERRUPTS()               \
  bool ___were_enabled_x = ((get_eflags() & 0x200) != 0); \
  disable_interrupts()                                 

#define CHECK_AND_TRY_ENABLE_INTERRUPTS()               \
  if (___were_enabled_x) {                              \
    enable_interrupts();                               \
  }

#endif // !__ANIVA_INTERRUPTS__

