#include "LibSys/event/key.h"
#include "dev/core.h"
#include "dev/debug/serial.h"
#include "dev/debug/test.h"
#include "dev/driver.h"
#include "intr/interrupts.h"
#include "kevent/event.h"
#include "kevent/types/keyboard.h"
#include "libk/flow/error.h"
#include "libk/io.h"
#include "libk/data/linkedlist.h"
#include "libk/stddef.h"
#include "libk/string.h"
#include "logging/log.h"
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

#define KBD_PORT_DATA 0x60
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
uintptr_t ps2_keyboard_msg(aniva_driver_t* this, dcc_t code, void* buffer, size_t size, void* out_buffer, size_t out_size);
registers_t* ps2_keyboard_irq_handler(registers_t* regs);

void set_flags(uint16_t* flags, uint8_t bit, bool val);

// TODO: finish this driver
const aniva_driver_t g_base_ps2_keyboard_driver = {
  .m_name = "ps2_kb",
  .m_type = DT_IO,
  .m_version = DRIVER_VERSION(0, 0, 1),
  .f_exit = ps2_keyboard_exit,
  .f_init = ps2_keyboard_entry,
  .f_msg = ps2_keyboard_msg,
};
EXPORT_DRIVER_PTR(g_base_ps2_keyboard_driver);

/*
 * This is okay right now, but once we create a HID device
 * for PS2, these variables need to be moved inside a device obj
 * so this is a FIXME
 */
static uint16_t s_mod_flags;
static uint16_t s_current_scancode;
static uint16_t s_current_scancodes[7];

int ps2_keyboard_entry() 
{
  ErrorOrPtr result;
  s_mod_flags = NULL;
  s_current_scancode = NULL;

  memset(&s_current_scancodes, 0, sizeof(s_current_scancodes));

  println("initializing ps2 keyboard driver!");

  result  = install_quick_int_handler(PS2_KB_IRQ_VEC, QIH_FLAG_REGISTERED | QIH_FLAG_BLOCK_EVENTS, I8259, ps2_keyboard_irq_handler);

  if (!IsError(result))
    quick_int_handler_enable_vector(PS2_KB_IRQ_VEC);

  /* Make sure the keyboard event isn't frozen */
  unfreeze_kevent("keyboard");

  return 0;
}

int ps2_keyboard_exit() 
{
  /* Make sure that the keyboard event is frozen, since there is no current kb driver */
  freeze_kevent("keyboard");
  uninstall_quick_int_handler(PS2_KB_IRQ_VEC);
  return 0;
}

uintptr_t ps2_keyboard_msg(aniva_driver_t* this, dcc_t code, void* buffer, size_t size, void* out_buffer, size_t out_size) 
{
  return 0;
}

static void ps2_set_keycode_buffer(uint16_t keycode, bool pressed)
{
  uint32_t key_idx;

  if (pressed) {
    for (uint32_t i = 0; i < arrlen(s_current_scancodes); i++) {
      /* No duplicates */
      if (s_current_scancodes[i] == keycode)
        break;

      /* Skip taken scancodes */
      if (s_current_scancodes[i])
        continue;

      s_current_scancodes[i] = keycode;
      break;
    }

    return;
  }

  key_idx = 0;

  /* Search for our keycode */
  for (; key_idx < arrlen(s_current_scancodes); key_idx++)
    if (s_current_scancodes[key_idx] == keycode)
      break;

  /* Shift the remaining keys forwards */
  while (key_idx < arrlen(s_current_scancodes)) {

    /* If we can't, simply put a NULL */
    if ((key_idx+1) >= arrlen(s_current_scancodes))
      s_current_scancodes[key_idx] = NULL;
    else
      s_current_scancodes[key_idx] = s_current_scancodes[key_idx+1];

    key_idx++;
  }
}

registers_t* ps2_keyboard_irq_handler(registers_t* regs) 
{
  char character;
  uint16_t scan_code = (uint16_t)(in8(0x60)) | s_current_scancode;
  bool pressed = (!(scan_code & 0x80));

  if (scan_code == 0xe0) {
    /* Extended keycode */
    s_current_scancode = scan_code;
    return regs;
  }
  
  s_current_scancode = NULL;

  uint16_t key_code = scan_code & 0x7f;

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

  character = NULL;

  if (key_code < 256)
    character = (s_mod_flags & KBD_MOD_SHIFT)
                 ? kbd_us_shift_map[key_code]
                 : kbd_us_map[key_code];

  kevent_kb_ctx_t kb = {
    .pressed = pressed,
    .keycode = aniva_scancode_table[key_code],
    .pressed_char = character,
  };

  /* Buffer the keycodes */
  ps2_set_keycode_buffer(kb.keycode, pressed);

  /* Copy the scancode buffer to the event */
  memcpy(&kb.pressed_keys, &s_current_scancodes, sizeof(s_current_scancodes));

  println(to_string(kb.pressed_keys[0]));
  println(to_string(kb.pressed_keys[1]));
  println(to_string(kb.pressed_keys[2]));
  println(to_string(kb.pressed_keys[3]));

  kevent_fire("keyboard", &kb, sizeof(kb));

  return regs;
}

void set_flags(uint16_t* flags, uint8_t bit, bool val) {
  if (val) {
    *flags |= bit;
  } else {
    *flags &= ~bit;
  }
}
