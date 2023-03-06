#include "ps2_keyboard.h"
#include "dev/core.h"
#include "dev/debug/serial.h"
#include "dev/driver.h"
#include "dev/framebuffer/framebuffer.h"
#include "interupts/control/pic.h"
#include "interupts/interupts.h"
#include "libk/error.h"
#include "libk/io.h"
#include "mem/heap.h"
#include "proc/core.h"
#include "system/processor/registers.h"

#define PS2_KB_IRQ_VEC 1

void ps2_keyboard_entry();
int ps2_keyboard_exit();
int ps2_keyboard_msg(char*, ...);
registers_t* ps2_keyboard_irq_handler(registers_t* regs);

// TODO: finish this driver
const aniva_driver_t g_base_ps2_keyboard_driver = {
  .m_name = "ps2 keyboard",
  .m_type = DT_IO,
  .m_ident = DRIVER_IDENT(0, 0),
  .m_version = DRIVER_VERSION(0, 0, 1),
  .f_exit = ps2_keyboard_exit,
  .f_init = ps2_keyboard_entry,
  .f_drv_msg = ps2_keyboard_msg,
  .m_port = 0
};

void ps2_keyboard_entry() {

  InterruptHandler_t* handler = create_interrupt_handler(PS2_KB_IRQ_VEC, I8259, ps2_keyboard_irq_handler);
  bool success = interrupts_add_handler(handler);

  if (success) {
    handler->m_controller->fControllerEnableVector(PS2_KB_IRQ_VEC);
  }

}
int ps2_keyboard_exit() {

  interrupts_remove_handler(PS2_KB_IRQ_VEC);
  return 0;
}

int ps2_keyboard_msg(char* fmt, ...) {

  return 0;
}

static uintptr_t y_index = 0;

registers_t* ps2_keyboard_irq_handler(registers_t* regs) {

  char c = in8(0x60);

  println("called keyboard driver!");

  draw_char(69, y_index, 'K');

  uintptr_t* data = kmalloc(sizeof(uintptr_t));
  *data = 1;
  send_packet_to_socket(1, data, sizeof(uintptr_t));

  y_index+= 9;

  return regs;
}
