#ifndef __ANIVA_DRV_I8042__
#define __ANIVA_DRV_I8042__

#include "libk/io.h"
#include <libk/stddef.h>

#define PS2_KB_IRQ_VEC 1

char kbd_us_map[256] = {
    0,
    '\033',
    '1',
    '2',
    '3',
    '4',
    '5',
    '6',
    '7',
    '8',
    '9',
    '0',
    '-',
    '=',
    0x08,
    '\t',
    'q',
    'w',
    'e',
    'r',
    't',
    'y',
    'u',
    'i',
    'o',
    'p',
    '[',
    ']',
    '\n',
    0,
    'a',
    's',
    'd',
    'f',
    'g',
    'h',
    'j',
    'k',
    'l',
    ';',
    '\'',
    '`',
    0,
    '\\',
    'z',
    'x',
    'c',
    'v',
    'b',
    'n',
    'm',
    ',',
    '.',
    '/',
    0,
    '*',
    0,
    ' ',
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    '-',
    0,
    0,
    0,
    '+',
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    '\\',
    0,
    0,
    0,
};

char kbd_us_shift_map[256] = {
    0,
    '\033',
    '!',
    '@',
    '#',
    '$',
    '%',
    '^',
    '&',
    '*',
    '(',
    ')',
    '_',
    '+',
    0x08,
    '\t',
    'Q',
    'W',
    'E',
    'R',
    'T',
    'Y',
    'U',
    'I',
    'O',
    'P',
    '{',
    '}',
    '\n',
    0,
    'A',
    'S',
    'D',
    'F',
    'G',
    'H',
    'J',
    'K',
    'L',
    ':',
    '\"',
    '~',
    0,
    '|',
    'Z',
    'X',
    'C',
    'V',
    'B',
    'N',
    'M',
    '<',
    '>',
    '?',
    0,
    '*',
    0,
    ' ',
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    '-',
    0,
    0,
    0,
    '+',
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    '|',
    0,
    0,
    0,
};

#define I8042_DATA_PORT 0x60
#define I8042_STATUS_PORT 0x64
#define I8042_CMD_PORT 0x64

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

static inline uint32_t i8042_read_data()
{
    return in8(I8042_DATA_PORT);
}

static inline uint32_t i8042_read_status()
{
    return in8(I8042_STATUS_PORT);
}

static inline void i8042_write_data(uint8_t byte)
{
    out8(I8042_DATA_PORT, byte);
}

static inline void i8042_write_cmd(uint8_t byte)
{
    out8(I8042_CMD_PORT, byte);
}

#endif // !__ANIVA_DRV_I8042__
