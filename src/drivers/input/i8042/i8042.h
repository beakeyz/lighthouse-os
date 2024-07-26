#ifndef __ANIVA_DRV_I8042__
#define __ANIVA_DRV_I8042__

#include "libk/io.h"
#include <libk/stddef.h>

#define PS2_KB_IRQ_VEC 1
#define PS2_MOUSE_IRQ_VEC 12

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

#define I8042_STATUS_OUTBUF_FULL 0x1u
#define I8042_STATUS_INBUF_FULL 0x2u
#define I8042_STATUS_SYSFLAG 0x4u
#define I8042_STATUS_CMDORDATA 0x8u
#define I8042_STATUS_WHICHBUF 0x20u
#define I8042_STATUS_TIMEOUT 0x40u
#define I8042_STATUS_PARITYERR 0x80u

#define I8042_CFG_PORT1_IRQ 0x01
#define I8042_CFG_PORT2_IRQ 0x02
#define I8042_CFG_PORT1_TRANSLATE 0x04

#define I8042_CMD_RD_CFG 0x20
#define I8042_CMD_WR_CFG 0x60
#define I8042_CMD_DISABLE_PORT2 0xA7
#define I8042_CMD_ENABLE_PORT2 0xA8
#define I8042_CMD_DISABLE_PORT1 0xAD
#define I8042_CMD_ENABLE_PORT1 0xAE
#define I8042_CMD_WRITE_MOUSE 0xD4

#define MOUSE_SET_REMOTE 0xF0
#define MOUSE_DEVICE_ID 0xF2
#define MOUSE_SAMPLE_RATE 0xF3
#define MOUSE_DATA_ON 0xF4
#define MOUSE_DATA_OFF 0xF5
#define MOUSE_SET_DEFAULTS 0xF6

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

#define MOUSE_STAT_LBTN 0x01
#define MOUSE_STAT_RBTN 0x02
#define MOUSE_STAT_MBTN 0x04
#define MOSUE_STAT_IDK 0x08
#define MOUSE_STAT_X_NEGATIVE 0x1
#define MOUSE_STAT_Y_NEGATIVE 0x20
#define MOUSE_STAT_X_OVERFLOW 0x40
#define MOUSE_STAT_Y_OVERFLOW 0x80

#endif // !__ANIVA_DRV_I8042__
