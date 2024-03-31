#include "dev/core.h"
#include "drivers/input/i8042/i8042.h"
#include "irq/interrupts.h"
#include "kevent/event.h"
#include "kevent/types/keyboard.h"
#include "lightos/event/key.h"
#include <dev/driver.h>
#include <dev/device.h>

/*
 * This is okay right now, but once we create a HID device
 * for PS2, these variables need to be moved inside a device obj
 * so this is a FIXME
 */
//static device_t* s_i8042_device;
static uint16_t s_mod_flags;
static uint16_t s_current_scancode;
static uint16_t s_current_scancodes[7];

registers_t* i8042_irq_handler(registers_t* regs);

static int _init_i8042()
{
  logln("Initalizing i8042 driver!");

  int error;
  s_mod_flags = NULL;
  s_current_scancode = NULL;

  memset(&s_current_scancodes, 0, sizeof(s_current_scancodes));

  /* Try to allocate an IRQ */
  error = irq_allocate(PS2_KB_IRQ_VEC, NULL, i8042_irq_handler, NULL, "PS/2 keyboard");

  if (error)
    return -1;

  /* Make sure the keyboard event isn't frozen */
  unfreeze_kevent("keyboard");

  return 0;
}

static int _exit_i8042()
{
  int error;

  /* Make sure that the keyboard event is frozen, since there is no current kb driver */
  freeze_kevent("keyboard");

  error = irq_deallocate(PS2_KB_IRQ_VEC);

  return error;
}

/*!
 * @brief: Called when we try to match this driver to a device
 *
 * @returns: 0 when the probe is successfull, otherwise a kerr code
 */
static kerror_t _probe_i8042(aniva_driver_t* driver, device_t* dev)
{
  return -KERR_INVAL;
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


static inline void set_flags(uint16_t* flags, uint8_t bit, bool val) 
{
  if (val) {
    *flags |= bit;
  } else {
    *flags &= ~bit;
  }
}

registers_t* i8042_irq_handler(registers_t* regs) 
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

  kevent_fire("keyboard", &kb, sizeof(kb));

  return regs;
}

EXPORT_DRIVER(i8042) = {
  .m_name = "i8042",
  .m_descriptor = "i8042 input driver",
  .m_type = DT_IO,
  .m_version = DRIVER_VERSION(0, 0, 1),
  .f_init = _init_i8042,
  .f_exit = _exit_i8042,
  .f_probe = _probe_i8042,
};

EXPORT_DEPENDENCIES(deps) = {
  //DRV_DEP(DRV_DEPTYPE_PATH, NULL, "Root/System/"),
  DRV_DEP_END,
};

