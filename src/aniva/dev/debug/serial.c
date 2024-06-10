#include "serial.h"
#include "logging/log.h"
#include <libk/io.h>
#include <libk/stddef.h>
#include <libk/string.h>

static bool has_serial = false;

static logger_t serial_logger = {
    .title = "qemu_serial",
    /* Let everything pass through serial */
    .flags = LOGGER_FLAG_DEBUG | LOGGER_FLAG_INFO | LOGGER_FLAG_WARNINGS,
    .f_logc = serial_putch,
    .f_log = serial_print,
    .f_logln = serial_println,
};

/*!
 * @brief Brutaly initialize QEMU (COM1) serial IF
 *
 * Make sure we are able to send data over the IO line and register
 * a logger to the logging subsystem
 */
void init_serial()
{

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

int serial_putch(char c)
{
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

int serial_print(const char* str)
{
    size_t x = 0;
    while (str[x] != '\0') {
        serial_putch(str[x++]);
    }

    return 0;
}

int serial_println(const char* str)
{
    size_t x = 0;
    while (str[x] != '\0') {
        serial_putch(str[x++]);
    }
    if (str[x - 1] != '\n') {
        serial_putch('\n');
    }
    return 0;
}
