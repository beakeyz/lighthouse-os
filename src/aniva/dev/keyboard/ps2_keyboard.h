#ifndef __ANIVA_PS2_KEYBOARD_DRIVER__
#define __ANIVA_PS2_KEYBOARD_DRIVER__

#include "dev/driver.h"

typedef struct ps2_key_event {
  uint32_t m_key_code;
  char m_typed_char;
  bool m_pressed;
} ps2_key_event_t;

typedef void (*ps2_key_callback) (ps2_key_event_t event);

#define KB_REGISTER_CALLBACK 70
#define KB_UNREGISTER_CALLBACK 71

const extern aniva_driver_t g_base_ps2_keyboard_driver;
#endif // !__ANIVA_PS2_KEYBOARD_DRIVER__
