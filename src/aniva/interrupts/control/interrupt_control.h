#ifndef __ANIVA_INTERRUPT_CONTROL__
#define __ANIVA_INTERRUPT_CONTROL__
#include "libk/data/linkedlist.h"
#include <libk/stddef.h>

typedef void (*INTERRUPT_CONTROLLER_EOI) (
  uint8_t irq_num
);

typedef void* (*INTERRUPT_CONTROLLER_INIT) (
);

typedef void (*INTERRUPT_CONTROLLER_DISABLE) (
  void* this
);

typedef void (*INTERRUPT_CONTROLLER_ENABLE_VECTOR) (
  uint8_t vector
);

typedef void (*INTERRUPT_CONTROLLER_DISABLE_VECTOR) (
  uint8_t vector
);

typedef enum {
  I8259 = 1,    /* INTEL DUAL PIC */
  I82093AA = 2, /* INTEL I/O APIC */ 
} INTERRUPT_CONTROLLER_TYPE;

// Represent the irq controller (PIC, IOAPIC, ect.)
typedef struct InterruptController {
  void* __parent; // pointer to its parent (SHOULD NOT BE USED OFTEN)
  INTERRUPT_CONTROLLER_TYPE m_type;

  INTERRUPT_CONTROLLER_EOI fInterruptEOI;
  INTERRUPT_CONTROLLER_INIT fControllerInit;
  INTERRUPT_CONTROLLER_DISABLE fControllerDisable;

  INTERRUPT_CONTROLLER_ENABLE_VECTOR fControllerEnableVector;
  INTERRUPT_CONTROLLER_DISABLE_VECTOR fControllerDisableVector;
  // TODO:
} InterruptController_t;

typedef struct {
  INTERRUPT_CONTROLLER_TYPE m_current_type;
  size_t m_controller_count;
  list_t* m_controllers;
} InterruptControllerManager_t;

void init_int_control_management();
void interrupt_control_switch_to_mode(INTERRUPT_CONTROLLER_TYPE type);
InterruptController_t* get_controller_for_int_number(uint16_t int_num);

extern InterruptControllerManager_t* g_interrupt_controller_manager;

#endif // !__ANIVA_INTERRUPT_CONTROL__
