#include "interupts.h"
#include "kernel/interupts/idt.h"
#include "kernel/mem/kmem_manager.h"
#include "libk/string.h"
#include <kernel/dev/debug/serial.h>
#include <kernel/interupts/control/pic.h>
#include <libk/stddef.h>

static irq_specific_handler_t handler_entries[16] = {NULL};

// yeahahhhhh
registers_t* interrupt_service_routine_0(registers_t *);
registers_t* interrupt_service_routine_1(registers_t *);
registers_t* interrupt_service_routine_2(registers_t *);
registers_t* interrupt_service_routine_3(registers_t *);
registers_t* interrupt_service_routine_4(registers_t *);
registers_t* interrupt_service_routine_5(registers_t *);
registers_t* interrupt_service_routine_6(registers_t *);
registers_t* interrupt_service_routine_7(registers_t *);
registers_t* interrupt_service_routine_error_code_8(registers_t *);
registers_t* interrupt_service_routine_9(registers_t *);
registers_t* interrupt_service_routine_error_code_10(registers_t *);
registers_t* interrupt_service_routine_error_code_11(registers_t *);
registers_t* interrupt_service_routine_error_code_12(registers_t *);
registers_t* interrupt_service_routine_error_code_13(registers_t *);
registers_t* interrupt_service_routine_error_code_14(registers_t *);
registers_t* interrupt_service_routine_15(registers_t *);
registers_t* interrupt_service_routine_16(registers_t *);
registers_t* interrupt_service_routine_error_code_17(registers_t *);
registers_t* interrupt_service_routine_18(registers_t *);
registers_t* irq_32(registers_t *);
registers_t* irq_33(registers_t *);
registers_t* irq_34(registers_t *);
registers_t* irq_35(registers_t *);
registers_t* irq_36(registers_t *);
registers_t* irq_37(registers_t *);
registers_t* irq_38(registers_t *);
registers_t* irq_39(registers_t *);
registers_t* irq_40(registers_t *);
registers_t* irq_41(registers_t *);
registers_t* irq_42(registers_t *);
registers_t* irq_43(registers_t *);
registers_t* irq_44(registers_t *);
registers_t* irq_45(registers_t *);
registers_t* irq_46(registers_t *);
registers_t* irq_47(registers_t *);

void init_interupts() {

  idt_set_gate(0x00, 0x8E, 0x08, interrupt_service_routine_0);
  idt_set_gate(0x01, 0x8E, 0x08, interrupt_service_routine_1);
  idt_set_gate(0x02, 0x8E, 0x08, interrupt_service_routine_2);
  idt_set_gate(0x03, 0x8E, 0x08, interrupt_service_routine_3);
  idt_set_gate(0x04, 0x8E, 0x08, interrupt_service_routine_4);
  idt_set_gate(0x05, 0x8E, 0x08, interrupt_service_routine_5);
  idt_set_gate(0x06, 0x8E, 0x08, interrupt_service_routine_6);
  idt_set_gate(0x07, 0x8E, 0x08, interrupt_service_routine_7);
  idt_set_gate(0x08, 0x8E, 0x08, interrupt_service_routine_error_code_8);
  idt_set_gate(0x09, 0x8E, 0x08, interrupt_service_routine_9);
  idt_set_gate(0x0A, 0x8E, 0x08, interrupt_service_routine_error_code_10);
  idt_set_gate(0x0B, 0x8E, 0x08, interrupt_service_routine_error_code_11);
  idt_set_gate(0x0C, 0x8E, 0x08, interrupt_service_routine_error_code_12);
  idt_set_gate(0x0D, 0x8E, 0x08, interrupt_service_routine_error_code_13);
  idt_set_gate(0x0E, 0x8E, 0x08, interrupt_service_routine_error_code_14);
  idt_set_gate(0x0F, 0x8E, 0x08, interrupt_service_routine_15);
  idt_set_gate(0x10, 0x8E, 0x08, interrupt_service_routine_16);
  idt_set_gate(0x11, 0x8E, 0x08, interrupt_service_routine_error_code_17);
  idt_set_gate(0x12, 0x8E, 0x08, interrupt_service_routine_18);
  idt_set_gate(0x20, 0x8E, 0x08, irq_32);
  idt_set_gate(0x21, 0x8E, 0x08, irq_33);
  idt_set_gate(0x22, 0x8E, 0x08, irq_34);
  idt_set_gate(0x23, 0x8E, 0x08, irq_35);
  idt_set_gate(0x24, 0x8E, 0x08, irq_36);
  idt_set_gate(0x25, 0x8E, 0x08, irq_37);
  idt_set_gate(0x26, 0x8E, 0x08, irq_38);
  idt_set_gate(0x27, 0x8E, 0x08, irq_39);
  idt_set_gate(0x28, 0x8E, 0x08, irq_40);
  idt_set_gate(0x29, 0x8E, 0x08, irq_41);
  idt_set_gate(0x2a, 0x8E, 0x08, irq_42);
  idt_set_gate(0x2b, 0x8E, 0x08, irq_43);
  idt_set_gate(0x2c, 0x8E, 0x08, irq_44);
  idt_set_gate(0x2d, 0x8E, 0x08, irq_45);
  idt_set_gate(0x2e, 0x8E, 0x08, irq_46);
  idt_set_gate(0x2f, 0x8E, 0x08, irq_47);

  // TODO: apic and shit?
  init_pic();
}

// FIXME: look into the idea of having a error datastruct for this type of
// stuff... otherwise return an int with statuscode?
void add_handler(size_t irq_num, irq_specific_handler_t handler_ptr) {
  disable_interupts();
  irq_specific_handler_t handler = handler_entries[irq_num];
  if (handler != nullptr) {
    // yikes, there already is a handler
    println("handler already exists");
    return;
  }

  if (&handler == &handler_ptr) {
    // yikes
    println("Same handler =/");
    return;
  }

  println("added handler");
  handler_entries[irq_num] = handler_ptr;
  enable_interupts();
}

void remove_handler(size_t irq_num) {
  disable_interupts();
  irq_specific_handler_t handler = handler_entries[irq_num];
  if (!handler) {
    // yikes
    return;
  }

  handler_entries[irq_num] = nullptr;
  enable_interupts();
}

// Traffic distribution function: finds the right handler for the right function
static registers_t *_int_handler(struct registers *regs, int num) {
  irq_specific_handler_t handler = handler_entries[num - 32];

  if (handler) {
    if (handler(regs)) {

      // ack and return
      pic_eoi(num);
      return regs;
    }
  }

  println("[INFO] no handler to catch interrupt");
  pic_eoi(num);
  return regs;
}

// main entrypoint for the interupts (from the asm)
registers_t *interrupt_handler(struct registers *regs) {
  // TODO: call _int_handler
  switch (regs->int_no) {
    // errors
    // TODO: panic handlers
    case 14:
      // pagefault
      println("hi bitches!");
      for (;;) {
        disable_interupts();
        asm volatile ("hlt");
      }

      break;

    // irqs lol
    case 32:
    case 33:
    case 34:
    case 35:
    case 36:
    case 37:
    case 38:
    case 39:
    case 40:
    case 41:
    case 42:
    case 43:
    case 44:
    case 45:
    case 46:
    case 47:
      _int_handler(regs, regs->int_no);
      return regs;
  }
  
  for (;;) {}
  return regs;
}

void disable_interupts() { asm volatile("cli"); }

void enable_interupts() { asm volatile("sti"); }
