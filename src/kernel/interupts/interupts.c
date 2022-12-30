#include "interupts.h"
#include "interupts/control/interrupt_control.h"
#include "kernel/interupts/idt.h"
#include "kernel/mem/kmem_manager.h"
#include "libk/error.h"
#include "libk/string.h"
#include <kernel/dev/debug/serial.h>
#include <kernel/interupts/control/pic.h>
#include <libk/stddef.h>

static InterruptHandler_t g_handlers[16] = {NULL};

void init_interupts() {
  // TODO: massive todo lol
  //        - find out what to do with exeptions
  //        - find out what to do with generic handlers
  //        - do we do function decleration in asm or c???
  //        - cry
}

// TODO: do stuff
void add_handler(size_t irq_num, irq_handler_t handler_ptr) {
  disable_interupts();

  enable_interupts();
}

void remove_handler(size_t irq_num) {
  disable_interupts();

  enable_interupts();
}

// main entrypoint for the interupts (from the asm)
registers_t *interrupt_handler(struct registers *regs) {
  // TODO: call _int_handler

  const uint8_t int_num = regs->int_no - 32;
  InterruptHandler_t* handler = &g_handlers[int_num];

  // TODO: surpious n crap
  if (handler && handler->m_is_registerd) {
    handler->m_interrupt->fHandler(regs);

    handler->m_controller->fInterruptEOI(int_num);
  }

  return regs;
}

void disable_interupts() { asm volatile("cli"); }

void enable_interupts() { asm volatile("sti"); }
