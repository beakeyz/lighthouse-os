#include "ps2_keyboard.h"
#include "dev/core.h"
#include "dev/debug/serial.h"
#include "dev/debug/test.h"
#include "dev/driver.h"
#include "dev/framebuffer/framebuffer.h"
#include "interupts/control/pic.h"
#include "interupts/interupts.h"
#include "libk/async_ptr.h"
#include "libk/error.h"
#include "libk/io.h"
#include "libk/linkedlist.h"
#include "libk/string.h"
#include "mem/heap.h"
#include "proc/core.h"
#include "proc/ipc/packet_response.h"

#define PS2_KB_IRQ_VEC 1

char kbd_us_map[256] = {
    0,   '\033', '1',  '2', '3',  '4', '5', '6', '7',  '8', '9', '0', '-',
    '=', 0x08,   '\t', 'q', 'w',  'e', 'r', 't', 'y',  'u', 'i', 'o', 'p',
    '[', ']',    '\n', 0,   'a',  's', 'd', 'f', 'g',  'h', 'j', 'k', 'l',
    ';', '\'',   '`',  0,   '\\', 'z', 'x', 'c', 'v',  'b', 'n', 'm', ',',
    '.', '/',    0,    '*', 0,    ' ', 0,   0,   0,    0,   0,   0,   0,
    0,   0,      0,    0,   0,    0,   0,   0,   0,    '-', 0,   0,   0,
    '+', 0,      0,    0,   0,    0,   0,   0,   '\\', 0,   0,   0,
};

char kbd_us_shift_map[256] = {
    0,   '\033', '!',  '@', '#', '$', '%', '^', '&', '*', '(', ')', '_',
    '+', 0x08,   '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P',
    '{', '}',    '\n', 0,   'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L',
    ':', '\"',   '~',  0,   '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<',
    '>', '?',    0,    '*', 0,   ' ', 0,   0,   0,   0,   0,   0,   0,
    0,   0,      0,    0,   0,   0,   0,   0,   0,   '-', 0,   0,   0,
    '+', 0,      0,    0,   0,   0,   0,   0,   '|', 0,   0,   0,
};

#define KBD_PORT_DATA
#define KBD_PORT_STATUS 0x64
#define KBD_PORT_COMMAND 0x64

#define KBD_STATUS_OUTBUF_FULL 0x1u
#define KBD_STATUS_INBUF_FULL 0x2u
#define KBD_STATUS_SYSFLAG 0x4u
#define KBD_STATUS_CMDORDATA 0x8u
#define KBD_STATUS_WHICHBUF 0x20u
#define KBD_STATUS_TIMEOUT 0x40u
#define KBD_STATUS_PARITYERR 0x80u

#define KBD_MOD_NONE 0x0u
#define KBD_MOD_ALT 0x1u
#define KBD_MOD_CTRL 0x2u
#define KBD_MOD_SHIFT 0x4u
#define KBD_MOD_SUPER 0x8u
#define KBD_MOD_ALTGR 0x10u
#define KBD_MOD_MASK 0x1Fu

#define KBD_SCANCODE_LSHIFT 0x2au
#define KBD_SCANCODE_RSHIFT 0x36u
#define KBD_SCANCODE_ALT 0x38u
#define KBD_SCANCODE_CTRL 0x1Du
#define KBD_SCANCODE_SUPER 0x5B
#define KBD_ACK 0xFAu

#define KBD_IS_PRESSED 0x80u

int ps2_keyboard_entry();
int ps2_keyboard_exit();
uintptr_t ps2_keyboard_msg(packet_payload_t payload, packet_response_t** response);
registers_t* ps2_keyboard_irq_handler(registers_t* regs);

void set_flags(uint16_t* flags, uint8_t bit, bool val);

// TODO: finish this driver
const aniva_driver_t g_base_ps2_keyboard_driver = {
  .m_name = "ps2_kb",
  .m_type = DT_IO,
  .m_ident = DRIVER_IDENT(0, 0),
  .m_version = DRIVER_VERSION(0, 0, 1),
  .f_exit = ps2_keyboard_exit,
  .f_init = ps2_keyboard_entry,
  .f_drv_msg = ps2_keyboard_msg,
};
EXPORT_DRIVER(g_base_ps2_keyboard_driver);

static list_t* s_kb_event_callbacks;
static uint16_t s_mod_flags;

int ps2_keyboard_entry() {

  s_kb_event_callbacks = init_list();
  s_mod_flags = NULL;

  InterruptHandler_t* handler = create_interrupt_handler(PS2_KB_IRQ_VEC, I8259, ps2_keyboard_irq_handler);
  bool success = interrupts_add_handler(handler);

  if (success) {
    handler->m_controller->fControllerEnableVector(PS2_KB_IRQ_VEC);
  }

  println("initializing ps2 keyboard driver!");
  return 0;
}
int ps2_keyboard_exit() {
  destroy_list(s_kb_event_callbacks);
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

  uint16_t scan_code = (uint16_t)in8(0x60);

  uint16_t key_code = scan_code & 0x7f;
  bool pressed = !(scan_code & 0x80);


  switch (key_code) {
    case KBD_SCANCODE_ALT:
      set_flags(&s_mod_flags, KBD_MOD_ALT, pressed);
      break;
    case KBD_SCANCODE_LSHIFT:
    case KBD_SCANCODE_RSHIFT:
      set_flags(&s_mod_flags, KBD_MOD_SHIFT, pressed);
      break;
    case KBD_SCANCODE_CTRL:
      set_flags(&s_mod_flags, KBD_MOD_CTRL, pressed);
      break;
    case KBD_SCANCODE_SUPER:
      set_flags(&s_mod_flags, KBD_MOD_SUPER, pressed);
      break;
  default:
    break;
  }

  char character = (s_mod_flags & KBD_MOD_SHIFT)
               ? kbd_us_shift_map[key_code]
               : kbd_us_map[key_code];

  FOREACH(i, s_kb_event_callbacks) {
    ps2_key_callback callback = i->data;

    ps2_key_event_t event = {
      .m_typed_char = character,
      .m_key_code = key_code,
      .m_pressed = pressed
    };

    callback(event);
  }

  return regs;
}

void set_flags(uint16_t* flags, uint8_t bit, bool val) {
  if (val) {
    *flags |= bit;
  } else {
    *flags &= ~bit;
  }
}
