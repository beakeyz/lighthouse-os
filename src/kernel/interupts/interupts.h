#ifndef __INTERUPTS__
#define __INTERUPTS__

#include <libk/stddef.h>

// NOTE: this file CAN NOT include idt.h, because of an include loop =/

struct Interrupt;
struct InterruptHandler;
struct InterruptController;

// handler func ptrs
typedef struct registers* (*interrupt_callback_t)(struct registers*);
typedef int (*irq_handler_t) (struct registers*);

typedef void (*INTERRUPT_CONTROLLER_EOI) (
  uint8_t irq_num
);

typedef enum {
  I8259 = 1,    /* INTEL DUAL PIC */
  I82093AA = 2, /* INTEL I/O APIC */ 
} INTERRUPT_CONTROLLER_TYPE;

typedef struct registers{
	uintptr_t r15, r14, r13, r12;
	uintptr_t r11, r10, r9, r8;
	uintptr_t rbp, rdi, rsi, rdx, rcx, rbx, rax;

	uintptr_t int_no, err_code;

	uintptr_t rip, cs, rflags, rsp, ss;
} __attribute__((packed)) registers_t;

// Represent the interrupt when fired
typedef struct Interrupt {
  irq_handler_t m_handler;
  uint32_t m_irq_num;
  INTERRUPT_CONTROLLER_TYPE m_type;
} Interrupt_t;

// Represent the interrupthandler that should handle a fired Interrupt_t
typedef struct InterruptHandler {
  Interrupt_t* m_interrupt;
  struct InterruptController* m_controller;
} InterruptHandler_t;

// Represent the irq controller (PIC, IOAPIC, ect.)
typedef struct InterruptController {
  INTERRUPT_CONTROLLER_TYPE m_type;

  INTERRUPT_CONTROLLER_EOI fInterruptEOI;
  // TODO:
} InterruptController_t;

InterruptController_t init_pic_controller();

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

