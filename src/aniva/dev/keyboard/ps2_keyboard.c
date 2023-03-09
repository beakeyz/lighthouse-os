#include "ps2_keyboard.h"
#include "dev/core.h"
#include "dev/debug/serial.h"
#include "dev/debug/test.h"
#include "dev/driver.h"
#include "dev/framebuffer/framebuffer.h"
#include "interupts/control/pic.h"
#include "interupts/interupts.h"
#include "kmain.h"
#include "libk/async_ptr.h"
#include "libk/error.h"
#include "libk/io.h"
#include "libk/linkedlist.h"
#include "libk/string.h"
#include "mem/heap.h"
#include "proc/core.h"
#include "proc/ipc/packet_response.h"

#define PS2_KB_IRQ_VEC 1

void ps2_keyboard_entry();
int ps2_keyboard_exit();
uintptr_t ps2_keyboard_msg(packet_payload_t payload, packet_response_t** response);
registers_t* ps2_keyboard_irq_handler(registers_t* regs);

// TODO: finish this driver
const aniva_driver_t g_base_ps2_keyboard_driver = {
  .m_name = "ps2_kb",
  .m_type = DT_IO,
  .m_ident = DRIVER_IDENT(0, 0),
  .m_version = DRIVER_VERSION(0, 0, 1),
  .f_exit = ps2_keyboard_exit,
  .f_init = ps2_keyboard_entry,
  .f_drv_msg = ps2_keyboard_msg,
  .m_port = 0
};

static list_t* s_kb_event_callbacks;

void ps2_keyboard_entry() {

  s_kb_event_callbacks = init_list();

  InterruptHandler_t* handler = create_interrupt_handler(PS2_KB_IRQ_VEC, I8259, ps2_keyboard_irq_handler);
  bool success = interrupts_add_handler(handler);

  if (success) {
    handler->m_controller->fControllerEnableVector(PS2_KB_IRQ_VEC);
  }

  println("initializing ps2 keyboard driver!");

}
int ps2_keyboard_exit() {

  interrupts_remove_handler(PS2_KB_IRQ_VEC);
  return 0;
}

uintptr_t ps2_keyboard_msg(packet_payload_t payload, packet_response_t** response) {

  switch (payload.m_code) {
    case KB_REGISTER_CALLBACK: {
      if (payload.m_data_size != sizeof(ps2_key_callback)) {
        // response?
        return -1;
      }
      ps2_key_callback callback = (ps2_key_callback)payload.m_data;
      list_append(s_kb_event_callbacks, callback);
      break;
    }
    case KB_UNREGISTER_CALLBACK: {
      if (payload.m_data_size != sizeof(ps2_key_callback)) {
        // response?
        return -1;
      }
      ps2_key_callback callback = payload.m_data;
      ErrorOrPtr index_result = list_indexof(s_kb_event_callbacks, callback);
      if (index_result.m_status == ANIVA_FAIL) {
        return -1;
      }
      list_remove(s_kb_event_callbacks, index_result.m_ptr);
      break;
    }
  }

  return 0;
}

static uintptr_t y_index = 0;

registers_t* ps2_keyboard_irq_handler(registers_t* regs) {

  char c = in8(0x60);

  FOREACH(i, s_kb_event_callbacks) {
    ps2_key_callback callback = i->data;

    ps2_key_event_t event = {
      .m_typed_char = c,
      .m_key_code = (uintptr_t)c
    };

    callback(event);
  }

  println("called keyboard driver!");

  return regs;
}
