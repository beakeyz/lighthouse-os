#include "serial.h"
#include "dev/debug/early_tty.h"
#include "entry/entry.h"
#include "intr/ctl/pic.h"
#include "libk/flow/error.h"
#include "logging/log.h"
#include "system/acpi/parser.h"
#include <libk/io.h>
#include <libk/stddef.h>
#include <libk/string.h>

static bool has_serial = false;

int serial_putch(char c);
int serial_print(const char* str);
int serial_println(const char* str);
int serial_printf(const char *fmt, ...);

static logger_t serial_logger = {
  .title = "qemu_serial",
  .flags = NULL,
  .f_logc = serial_putch,
  .f_log = serial_print,
  .f_logln = serial_println,
  .f_logf = serial_printf,
};

/*!
 * @brief Brutaly initialize QEMU (COM1) serial IF
 *
 * Make sure we are able to send data over the IO line and register
 * a logger to the logging subsystem
 */
void init_serial() {

  out8(COM1 + 1, 0x00);
  out8(COM1 + 3, 0x80);
  out8(COM1 + 0, 0x02);
  out8(COM1 + 1, 0x00);
  out8(COM1 + 3, 0x03);
  out8(COM1 + 2, 0xC7);
  out8(COM1 + 4, 0x0B);

  // FIXME: test the serial bus???
  
  has_serial = true;

  register_logger(&serial_logger);
}

int serial_putch(char c) {
  if (!has_serial) {
      return -1;
  }
  static int was_cr = false;
  
  while ((in8(COM1 + 5) & 0x20) == 0) {
    io_delay();
  }
  
  if (c == '\n' && !was_cr)
      out8(COM1, '\r');

  out8(COM1, c);

  was_cr = c == '\r';

  return 0;
}

int serial_print(const char* str) {
  size_t x = 0;
  while (str[x] != '\0') {
      serial_putch(str[x++]);
  }

  if (g_system_info.sys_flags & SYSFLAGS_HAS_EARLY_TTY) {
    etty_print((char*)str);
  }

  return 0;
}

int serial_println(const char* str) {
  size_t x = 0;
  while (str[x] != '\0') {
      serial_putch(str[x++]);
  }
  if (str[x-1] != '\n') {
      serial_putch('\n');
  }

  if (g_system_info.sys_flags & SYSFLAGS_HAS_EARLY_TTY) {
    etty_println((char*)str);
  }

  return 0;
}

int serial_printf(const char *fmt, ...)
{
  kernel_panic("TODO: kernel printf");
  return 0;
}
