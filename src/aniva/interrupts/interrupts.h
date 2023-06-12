#ifndef __ANIVA_INTERRUPTS__
#define __ANIVA_INTERRUPTS__

#include "interrupts/control/interrupt_control.h"
#include "libk/error.h"
#include "system/processor/registers.h"
#include <libk/stddef.h>

// NOTE: this file CAN NOT include idt.h, because of an include loop =/

#define IRQ_VEC_BASE 32
#define INTERRUPT_HANDLER_COUNT (256 - IRQ_VEC_BASE)

struct Interrupt;
struct quick_interrupthandler;

// handler func ptrs
typedef void (*interrupt_exeption_t) ();
typedef void (*interrupt_error_handler_t)(registers_t*);
typedef registers_t* (*interrupt_callback_t)(registers_t*);

#define QIH_FLAG_REGISTERED         (0x00000001)
#define QIH_FLAG_IN_INTERRUPT       (0x00000002)
#define QIH_FLAG_BLOCK_EVENTS       (0x00000004)

typedef struct quick_interrupt_handler {
  interrupt_callback_t fHandler;
  uint32_t m_int_num;
  uint32_t m_flags;
  InterruptController_t* m_controller;
} quick_interrupthandler_t;

/*
 * Fill a predefined space based on the int_num for a quick interrupt handler
 */
ErrorOrPtr install_quick_int_handler(uint32_t int_num, uint32_t flags, INTERRUPT_CONTROLLER_TYPE type, interrupt_callback_t callback);

/*
 * Zero out the quick handler at spot int_num
 */
void uninstall_quick_int_handler(uint32_t int_num);

ErrorOrPtr quick_int_handler_enable_vector(uint32_t int_num);
ErrorOrPtr quick_int_handler_disable_vector(uint32_t int_num);

size_t interrupts_get_handler_count(uint32_t int_num);

/*
 * Initialize the interrupt subsystem
 */
void init_interrupts();

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

