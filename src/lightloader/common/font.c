#include "heap.h"
#include "stddef.h"
#include <font.h>
#include <memory.h>

light_font_t default_light_font = {
    .width = 8,
    .height = 16,
    .bytes_per_glyph = 16,
    .glyph_count = 0xFF,
    .data = {
        // Credit goes to https://github.com/apple/darwin-xnu/blob/main/osfmk/console/iso_font.c
        /*   0 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /*   1 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /*   2 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /*   3 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /*   4 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /*   5 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /*   6 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /*   7 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /*   8 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /*   9 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /*  10 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /*  11 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /*  12 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /*  13 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /*  14 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /*  15 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /*  16 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /*  17 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /*  18 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /*  19 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /*  20 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /*  21 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /*  22 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /*  23 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /*  24 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /*  25 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /*  26 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /*  27 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /*  28 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /*  29 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /*  30 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /*  31 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /*  32 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /*  33 */ 0x00, 0x00, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00,
        /*  34 */ 0x00, 0x00, 0x6c, 0x6c, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /*  35 */ 0x00, 0x00, 0x00, 0x36, 0x36, 0x7f, 0x36, 0x36, 0x7f, 0x36, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00,
        /*  36 */ 0x00, 0x08, 0x08, 0x3e, 0x6b, 0x0b, 0x0b, 0x3e, 0x68, 0x68, 0x6b, 0x3e, 0x08, 0x08, 0x00, 0x00,
        /*  37 */ 0x00, 0x00, 0x00, 0x33, 0x13, 0x18, 0x08, 0x0c, 0x04, 0x06, 0x32, 0x33, 0x00, 0x00, 0x00, 0x00,
        /*  38 */ 0x00, 0x00, 0x1c, 0x36, 0x36, 0x1c, 0x6c, 0x3e, 0x33, 0x33, 0x7b, 0xce, 0x00, 0x00, 0x00, 0x00,
        /*  39 */ 0x00, 0x00, 0x18, 0x18, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /*  40 */ 0x00, 0x00, 0x30, 0x18, 0x18, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x18, 0x18, 0x30, 0x00, 0x00, 0x00,
        /*  41 */ 0x00, 0x00, 0x0c, 0x18, 0x18, 0x30, 0x30, 0x30, 0x30, 0x30, 0x18, 0x18, 0x0c, 0x00, 0x00, 0x00,
        /*  42 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x36, 0x1c, 0x7f, 0x1c, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /*  43 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x7e, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /*  44 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x0c, 0x00, 0x00, 0x00,
        /*  45 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /*  46 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00,
        /*  47 */ 0x00, 0x00, 0x60, 0x20, 0x30, 0x10, 0x18, 0x08, 0x0c, 0x04, 0x06, 0x02, 0x03, 0x00, 0x00, 0x00,
        /*  48 */ 0x00, 0x00, 0x3e, 0x63, 0x63, 0x63, 0x6b, 0x6b, 0x63, 0x63, 0x63, 0x3e, 0x00, 0x00, 0x00, 0x00,
        /*  49 */ 0x00, 0x00, 0x18, 0x1e, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00,
        /*  50 */ 0x00, 0x00, 0x3e, 0x63, 0x60, 0x60, 0x30, 0x18, 0x0c, 0x06, 0x03, 0x7f, 0x00, 0x00, 0x00, 0x00,
        /*  51 */ 0x00, 0x00, 0x3e, 0x63, 0x60, 0x60, 0x3c, 0x60, 0x60, 0x60, 0x63, 0x3e, 0x00, 0x00, 0x00, 0x00,
        /*  52 */ 0x00, 0x00, 0x30, 0x38, 0x3c, 0x36, 0x33, 0x7f, 0x30, 0x30, 0x30, 0x30, 0x00, 0x00, 0x00, 0x00,
        /*  53 */ 0x00, 0x00, 0x7f, 0x03, 0x03, 0x3f, 0x60, 0x60, 0x60, 0x60, 0x63, 0x3e, 0x00, 0x00, 0x00, 0x00,
        /*  54 */ 0x00, 0x00, 0x3c, 0x06, 0x03, 0x03, 0x3f, 0x63, 0x63, 0x63, 0x63, 0x3e, 0x00, 0x00, 0x00, 0x00,
        /*  55 */ 0x00, 0x00, 0x7f, 0x60, 0x30, 0x30, 0x18, 0x18, 0x18, 0x0c, 0x0c, 0x0c, 0x00, 0x00, 0x00, 0x00,
        /*  56 */ 0x00, 0x00, 0x3e, 0x63, 0x63, 0x63, 0x3e, 0x63, 0x63, 0x63, 0x63, 0x3e, 0x00, 0x00, 0x00, 0x00,
        /*  57 */ 0x00, 0x00, 0x3e, 0x63, 0x63, 0x63, 0x7e, 0x60, 0x60, 0x60, 0x30, 0x1e, 0x00, 0x00, 0x00, 0x00,
        /*  58 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00,
        /*  59 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x18, 0x18, 0x0c, 0x00, 0x00, 0x00,
        /*  60 */ 0x00, 0x00, 0x60, 0x30, 0x18, 0x0c, 0x06, 0x06, 0x0c, 0x18, 0x30, 0x60, 0x00, 0x00, 0x00, 0x00,
        /*  61 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x7e, 0x00, 0x00, 0x7e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /*  62 */ 0x00, 0x00, 0x06, 0x0c, 0x18, 0x30, 0x60, 0x60, 0x30, 0x18, 0x0c, 0x06, 0x00, 0x00, 0x00, 0x00,
        /*  63 */ 0x00, 0x00, 0x3e, 0x63, 0x60, 0x30, 0x30, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00,
        /*  64 */ 0x00, 0x00, 0x3c, 0x66, 0x73, 0x7b, 0x6b, 0x6b, 0x7b, 0x33, 0x06, 0x3c, 0x00, 0x00, 0x00, 0x00,
        /*  65 */ 0x00, 0x00, 0x3e, 0x63, 0x63, 0x63, 0x7f, 0x63, 0x63, 0x63, 0x63, 0x63, 0x00, 0x00, 0x00, 0x00,
        /*  66 */ 0x00, 0x00, 0x3f, 0x63, 0x63, 0x63, 0x3f, 0x63, 0x63, 0x63, 0x63, 0x3f, 0x00, 0x00, 0x00, 0x00,
        /*  67 */ 0x00, 0x00, 0x3c, 0x66, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x66, 0x3c, 0x00, 0x00, 0x00, 0x00,
        /*  68 */ 0x00, 0x00, 0x1f, 0x33, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x33, 0x1f, 0x00, 0x00, 0x00, 0x00,
        /*  69 */ 0x00, 0x00, 0x7f, 0x03, 0x03, 0x03, 0x3f, 0x03, 0x03, 0x03, 0x03, 0x7f, 0x00, 0x00, 0x00, 0x00,
        /*  70 */ 0x00, 0x00, 0x7f, 0x03, 0x03, 0x03, 0x3f, 0x03, 0x03, 0x03, 0x03, 0x03, 0x00, 0x00, 0x00, 0x00,
        /*  71 */ 0x00, 0x00, 0x3c, 0x66, 0x03, 0x03, 0x03, 0x73, 0x63, 0x63, 0x66, 0x7c, 0x00, 0x00, 0x00, 0x00,
        /*  72 */ 0x00, 0x00, 0x63, 0x63, 0x63, 0x63, 0x7f, 0x63, 0x63, 0x63, 0x63, 0x63, 0x00, 0x00, 0x00, 0x00,
        /*  73 */ 0x00, 0x00, 0x3c, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3c, 0x00, 0x00, 0x00, 0x00,
        /*  74 */ 0x00, 0x00, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x33, 0x1e, 0x00, 0x00, 0x00, 0x00,
        /*  75 */ 0x00, 0x00, 0x63, 0x33, 0x1b, 0x0f, 0x07, 0x07, 0x0f, 0x1b, 0x33, 0x63, 0x00, 0x00, 0x00, 0x00,
        /*  76 */ 0x00, 0x00, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x7f, 0x00, 0x00, 0x00, 0x00,
        /*  77 */ 0x00, 0x00, 0x63, 0x63, 0x77, 0x7f, 0x7f, 0x6b, 0x6b, 0x63, 0x63, 0x63, 0x00, 0x00, 0x00, 0x00,
        /*  78 */ 0x00, 0x00, 0x63, 0x63, 0x67, 0x6f, 0x6f, 0x7b, 0x7b, 0x73, 0x63, 0x63, 0x00, 0x00, 0x00, 0x00,
        /*  79 */ 0x00, 0x00, 0x3e, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x3e, 0x00, 0x00, 0x00, 0x00,
        /*  80 */ 0x00, 0x00, 0x3f, 0x63, 0x63, 0x63, 0x63, 0x3f, 0x03, 0x03, 0x03, 0x03, 0x00, 0x00, 0x00, 0x00,
        /*  81 */ 0x00, 0x00, 0x3e, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x6f, 0x7b, 0x3e, 0x30, 0x60, 0x00, 0x00,
        /*  82 */ 0x00, 0x00, 0x3f, 0x63, 0x63, 0x63, 0x63, 0x3f, 0x1b, 0x33, 0x63, 0x63, 0x00, 0x00, 0x00, 0x00,
        /*  83 */ 0x00, 0x00, 0x3e, 0x63, 0x03, 0x03, 0x0e, 0x38, 0x60, 0x60, 0x63, 0x3e, 0x00, 0x00, 0x00, 0x00,
        /*  84 */ 0x00, 0x00, 0x7e, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00,
        /*  85 */ 0x00, 0x00, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x3e, 0x00, 0x00, 0x00, 0x00,
        /*  86 */ 0x00, 0x00, 0x63, 0x63, 0x63, 0x63, 0x63, 0x36, 0x36, 0x1c, 0x1c, 0x08, 0x00, 0x00, 0x00, 0x00,
        /*  87 */ 0x00, 0x00, 0x63, 0x63, 0x6b, 0x6b, 0x6b, 0x6b, 0x7f, 0x36, 0x36, 0x36, 0x00, 0x00, 0x00, 0x00,
        /*  88 */ 0x00, 0x00, 0x63, 0x63, 0x36, 0x36, 0x1c, 0x1c, 0x36, 0x36, 0x63, 0x63, 0x00, 0x00, 0x00, 0x00,
        /*  89 */ 0x00, 0x00, 0xc3, 0xc3, 0x66, 0x66, 0x3c, 0x3c, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00,
        /*  90 */ 0x00, 0x00, 0x7f, 0x30, 0x30, 0x18, 0x18, 0x0c, 0x0c, 0x06, 0x06, 0x7f, 0x00, 0x00, 0x00, 0x00,
        /*  91 */ 0x00, 0x00, 0x3c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x3c, 0x00, 0x00, 0x00, 0x00,
        /*  92 */ 0x00, 0x00, 0x03, 0x02, 0x06, 0x04, 0x0c, 0x08, 0x18, 0x10, 0x30, 0x20, 0x60, 0x00, 0x00, 0x00,
        /*  93 */ 0x00, 0x00, 0x3c, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x3c, 0x00, 0x00, 0x00, 0x00,
        /*  94 */ 0x00, 0x08, 0x1c, 0x36, 0x63, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /*  95 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00,
        /*  96 */ 0x00, 0x00, 0x0c, 0x0c, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /*  97 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x60, 0x7e, 0x63, 0x63, 0x73, 0x6e, 0x00, 0x00, 0x00, 0x00,
        /*  98 */ 0x00, 0x00, 0x03, 0x03, 0x03, 0x3b, 0x67, 0x63, 0x63, 0x63, 0x67, 0x3b, 0x00, 0x00, 0x00, 0x00,
        /*  99 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x63, 0x03, 0x03, 0x03, 0x63, 0x3e, 0x00, 0x00, 0x00, 0x00,
        /* 100 */ 0x00, 0x00, 0x60, 0x60, 0x60, 0x6e, 0x73, 0x63, 0x63, 0x63, 0x73, 0x6e, 0x00, 0x00, 0x00, 0x00,
        /* 101 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x63, 0x63, 0x7f, 0x03, 0x63, 0x3e, 0x00, 0x00, 0x00, 0x00,
        /* 102 */ 0x00, 0x00, 0x3c, 0x66, 0x06, 0x1f, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x00, 0x00, 0x00, 0x00,
        /* 103 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x6e, 0x73, 0x63, 0x63, 0x63, 0x73, 0x6e, 0x60, 0x63, 0x3e, 0x00,
        /* 104 */ 0x00, 0x00, 0x03, 0x03, 0x03, 0x3b, 0x67, 0x63, 0x63, 0x63, 0x63, 0x63, 0x00, 0x00, 0x00, 0x00,
        /* 105 */ 0x00, 0x00, 0x0c, 0x0c, 0x00, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x38, 0x00, 0x00, 0x00, 0x00,
        /* 106 */ 0x00, 0x00, 0x30, 0x30, 0x00, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x33, 0x1e, 0x00,
        /* 107 */ 0x00, 0x00, 0x03, 0x03, 0x03, 0x63, 0x33, 0x1b, 0x0f, 0x1f, 0x33, 0x63, 0x00, 0x00, 0x00, 0x00,
        /* 108 */ 0x00, 0x00, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x38, 0x00, 0x00, 0x00, 0x00,
        /* 109 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x35, 0x6b, 0x6b, 0x6b, 0x6b, 0x6b, 0x6b, 0x00, 0x00, 0x00, 0x00,
        /* 110 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x3b, 0x67, 0x63, 0x63, 0x63, 0x63, 0x63, 0x00, 0x00, 0x00, 0x00,
        /* 111 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x63, 0x63, 0x63, 0x63, 0x63, 0x3e, 0x00, 0x00, 0x00, 0x00,
        /* 112 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x3b, 0x67, 0x63, 0x63, 0x63, 0x67, 0x3b, 0x03, 0x03, 0x03, 0x00,
        /* 113 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x6e, 0x73, 0x63, 0x63, 0x63, 0x73, 0x6e, 0x60, 0xe0, 0x60, 0x00,
        /* 114 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x3b, 0x67, 0x03, 0x03, 0x03, 0x03, 0x03, 0x00, 0x00, 0x00, 0x00,
        /* 115 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x63, 0x0e, 0x38, 0x60, 0x63, 0x3e, 0x00, 0x00, 0x00, 0x00,
        /* 116 */ 0x00, 0x00, 0x00, 0x0c, 0x0c, 0x3e, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x38, 0x00, 0x00, 0x00, 0x00,
        /* 117 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x63, 0x63, 0x63, 0x63, 0x63, 0x73, 0x6e, 0x00, 0x00, 0x00, 0x00,
        /* 118 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x63, 0x63, 0x36, 0x36, 0x1c, 0x1c, 0x08, 0x00, 0x00, 0x00, 0x00,
        /* 119 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x63, 0x6b, 0x6b, 0x6b, 0x3e, 0x36, 0x36, 0x00, 0x00, 0x00, 0x00,
        /* 120 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x63, 0x36, 0x1c, 0x1c, 0x1c, 0x36, 0x63, 0x00, 0x00, 0x00, 0x00,
        /* 121 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x63, 0x63, 0x36, 0x36, 0x1c, 0x1c, 0x0c, 0x0c, 0x06, 0x03, 0x00,
        /* 122 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x7f, 0x60, 0x30, 0x18, 0x0c, 0x06, 0x7f, 0x00, 0x00, 0x00, 0x00,
        /* 123 */ 0x00, 0x00, 0x70, 0x18, 0x18, 0x18, 0x18, 0x0e, 0x18, 0x18, 0x18, 0x18, 0x70, 0x00, 0x00, 0x00,
        /* 124 */ 0x00, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00,
        /* 125 */ 0x00, 0x00, 0x0e, 0x18, 0x18, 0x18, 0x18, 0x70, 0x18, 0x18, 0x18, 0x18, 0x0e, 0x00, 0x00, 0x00,
        /* 126 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x6e, 0x3b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /* 127 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /* 128 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /* 129 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /* 130 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /* 131 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /* 132 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /* 133 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /* 134 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /* 135 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /* 136 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /* 137 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /* 138 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /* 139 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /* 140 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /* 141 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /* 142 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /* 143 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /* 144 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /* 145 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /* 146 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /* 147 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /* 148 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /* 149 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /* 150 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /* 151 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /* 152 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /* 153 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /* 154 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /* 155 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /* 156 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /* 157 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /* 158 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /* 159 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /* 160 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /* 161 */ 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00,
        /* 162 */ 0x00, 0x00, 0x00, 0x08, 0x08, 0x3e, 0x6b, 0x0b, 0x0b, 0x0b, 0x6b, 0x3e, 0x08, 0x08, 0x00, 0x00,
        /* 163 */ 0x00, 0x00, 0x1c, 0x36, 0x06, 0x06, 0x1f, 0x06, 0x06, 0x07, 0x6f, 0x3b, 0x00, 0x00, 0x00, 0x00,
        /* 164 */ 0x00, 0x00, 0x00, 0x00, 0x66, 0x3c, 0x66, 0x66, 0x66, 0x3c, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00,
        /* 165 */ 0x00, 0x00, 0xc3, 0xc3, 0x66, 0x66, 0x3c, 0x7e, 0x18, 0x7e, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00,
        /* 166 */ 0x00, 0x00, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00,
        /* 167 */ 0x00, 0x3c, 0x66, 0x0c, 0x1e, 0x33, 0x63, 0x66, 0x3c, 0x18, 0x33, 0x1e, 0x00, 0x00, 0x00, 0x00,
        /* 168 */ 0x00, 0x00, 0x36, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /* 169 */ 0x00, 0x00, 0x3c, 0x42, 0x99, 0xa5, 0x85, 0xa5, 0x99, 0x42, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x00,
        /* 170 */ 0x00, 0x1e, 0x30, 0x3e, 0x33, 0x3b, 0x36, 0x00, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /* 171 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x6c, 0x36, 0x1b, 0x1b, 0x36, 0x6c, 0x00, 0x00, 0x00, 0x00, 0x00,
        /* 172 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7f, 0x60, 0x60, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /* 173 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /* 174 */ 0x00, 0x00, 0x3c, 0x42, 0x9d, 0xa5, 0x9d, 0xa5, 0xa5, 0x42, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x00,
        /* 175 */ 0x00, 0x7e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /* 176 */ 0x00, 0x00, 0x1c, 0x36, 0x36, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /* 177 */ 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x7e, 0x18, 0x18, 0x00, 0x7e, 0x00, 0x00, 0x00, 0x00, 0x00,
        /* 178 */ 0x00, 0x1e, 0x33, 0x18, 0x0c, 0x06, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /* 179 */ 0x00, 0x1e, 0x33, 0x18, 0x30, 0x33, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /* 180 */ 0x00, 0x30, 0x18, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /* 181 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x66, 0x76, 0x6e, 0x06, 0x06, 0x03, 0x00,
        /* 182 */ 0x00, 0x00, 0x7e, 0x2f, 0x2f, 0x2f, 0x2e, 0x28, 0x28, 0x28, 0x28, 0x28, 0x00, 0x00, 0x00, 0x00,
        /* 183 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /* 184 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x30, 0x1e, 0x00,
        /* 185 */ 0x00, 0x0c, 0x0e, 0x0c, 0x0c, 0x0c, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /* 186 */ 0x00, 0x1e, 0x33, 0x33, 0x33, 0x33, 0x1e, 0x00, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /* 187 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x1b, 0x36, 0x6c, 0x6c, 0x36, 0x1b, 0x00, 0x00, 0x00, 0x00, 0x00,
        /* 188 */ 0x00, 0x10, 0x1c, 0x18, 0x18, 0x18, 0x00, 0x7f, 0x00, 0x18, 0x1c, 0x1a, 0x3e, 0x18, 0x00, 0x00,
        /* 189 */ 0x00, 0x10, 0x1c, 0x18, 0x18, 0x18, 0x00, 0x7f, 0x00, 0x1c, 0x36, 0x18, 0x0c, 0x3e, 0x00, 0x00,
        /* 190 */ 0x00, 0x1c, 0x36, 0x18, 0x36, 0x1c, 0x00, 0x7f, 0x00, 0x18, 0x1c, 0x1a, 0x3e, 0x18, 0x00, 0x00,
        /* 191 */ 0x00, 0x00, 0x00, 0x00, 0x0c, 0x0c, 0x00, 0x0c, 0x0c, 0x06, 0x06, 0x03, 0x63, 0x3e, 0x00, 0x00,
        /* 192 */ 0x0c, 0x18, 0x3e, 0x63, 0x63, 0x63, 0x7f, 0x63, 0x63, 0x63, 0x63, 0x63, 0x00, 0x00, 0x00, 0x00,
        /* 193 */ 0x18, 0x0c, 0x3e, 0x63, 0x63, 0x63, 0x7f, 0x63, 0x63, 0x63, 0x63, 0x63, 0x00, 0x00, 0x00, 0x00,
        /* 194 */ 0x08, 0x14, 0x3e, 0x63, 0x63, 0x63, 0x7f, 0x63, 0x63, 0x63, 0x63, 0x63, 0x00, 0x00, 0x00, 0x00,
        /* 195 */ 0x6e, 0x3b, 0x3e, 0x63, 0x63, 0x63, 0x7f, 0x63, 0x63, 0x63, 0x63, 0x63, 0x00, 0x00, 0x00, 0x00,
        /* 196 */ 0x36, 0x00, 0x3e, 0x63, 0x63, 0x63, 0x7f, 0x63, 0x63, 0x63, 0x63, 0x63, 0x00, 0x00, 0x00, 0x00,
        /* 197 */ 0x1c, 0x36, 0x3e, 0x63, 0x63, 0x63, 0x7f, 0x63, 0x63, 0x63, 0x63, 0x63, 0x00, 0x00, 0x00, 0x00,
        /* 198 */ 0x00, 0x00, 0xfe, 0x33, 0x33, 0x33, 0xff, 0x33, 0x33, 0x33, 0x33, 0xf3, 0x00, 0x00, 0x00, 0x00,
        /* 199 */ 0x00, 0x00, 0x3c, 0x66, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x66, 0x3c, 0x18, 0x30, 0x1e, 0x00,
        /* 200 */ 0x0c, 0x18, 0x7f, 0x03, 0x03, 0x03, 0x3f, 0x03, 0x03, 0x03, 0x03, 0x7f, 0x00, 0x00, 0x00, 0x00,
        /* 201 */ 0x18, 0x0c, 0x7f, 0x03, 0x03, 0x03, 0x3f, 0x03, 0x03, 0x03, 0x03, 0x7f, 0x00, 0x00, 0x00, 0x00,
        /* 202 */ 0x08, 0x14, 0x7f, 0x03, 0x03, 0x03, 0x3f, 0x03, 0x03, 0x03, 0x03, 0x7f, 0x00, 0x00, 0x00, 0x00,
        /* 203 */ 0x36, 0x00, 0x7f, 0x03, 0x03, 0x03, 0x3f, 0x03, 0x03, 0x03, 0x03, 0x7f, 0x00, 0x00, 0x00, 0x00,
        /* 204 */ 0x0c, 0x18, 0x3c, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3c, 0x00, 0x00, 0x00, 0x00,
        /* 205 */ 0x30, 0x18, 0x3c, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3c, 0x00, 0x00, 0x00, 0x00,
        /* 206 */ 0x18, 0x24, 0x3c, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3c, 0x00, 0x00, 0x00, 0x00,
        /* 207 */ 0x66, 0x00, 0x3c, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3c, 0x00, 0x00, 0x00, 0x00,
        /* 208 */ 0x00, 0x00, 0x1e, 0x36, 0x66, 0x66, 0x6f, 0x66, 0x66, 0x66, 0x36, 0x1e, 0x00, 0x00, 0x00, 0x00,
        /* 209 */ 0x6e, 0x3b, 0x63, 0x63, 0x67, 0x6f, 0x6f, 0x7b, 0x7b, 0x73, 0x63, 0x63, 0x00, 0x00, 0x00, 0x00,
        /* 210 */ 0x06, 0x0c, 0x3e, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x3e, 0x00, 0x00, 0x00, 0x00,
        /* 211 */ 0x30, 0x18, 0x3e, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x3e, 0x00, 0x00, 0x00, 0x00,
        /* 212 */ 0x08, 0x14, 0x3e, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x3e, 0x00, 0x00, 0x00, 0x00,
        /* 213 */ 0x6e, 0x3b, 0x3e, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x3e, 0x00, 0x00, 0x00, 0x00,
        /* 214 */ 0x36, 0x00, 0x3e, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x3e, 0x00, 0x00, 0x00, 0x00,
        /* 215 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x66, 0x3c, 0x18, 0x3c, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /* 216 */ 0x00, 0x20, 0x3e, 0x73, 0x73, 0x6b, 0x6b, 0x6b, 0x6b, 0x67, 0x67, 0x3e, 0x02, 0x00, 0x00, 0x00,
        /* 217 */ 0x0c, 0x18, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x3e, 0x00, 0x00, 0x00, 0x00,
        /* 218 */ 0x18, 0x0c, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x3e, 0x00, 0x00, 0x00, 0x00,
        /* 219 */ 0x08, 0x14, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x3e, 0x00, 0x00, 0x00, 0x00,
        /* 220 */ 0x36, 0x00, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x3e, 0x00, 0x00, 0x00, 0x00,
        /* 221 */ 0x30, 0x18, 0xc3, 0xc3, 0x66, 0x66, 0x3c, 0x3c, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00,
        /* 222 */ 0x00, 0x00, 0x0f, 0x06, 0x3e, 0x66, 0x66, 0x66, 0x66, 0x3e, 0x06, 0x0f, 0x00, 0x00, 0x00, 0x00,
        /* 223 */ 0x00, 0x00, 0x1e, 0x33, 0x33, 0x1b, 0x33, 0x63, 0x63, 0x63, 0x63, 0x3b, 0x00, 0x00, 0x00, 0x00,
        /* 224 */ 0x00, 0x0c, 0x18, 0x30, 0x00, 0x3e, 0x60, 0x7e, 0x63, 0x63, 0x73, 0x6e, 0x00, 0x00, 0x00, 0x00,
        /* 225 */ 0x00, 0x30, 0x18, 0x0c, 0x00, 0x3e, 0x60, 0x7e, 0x63, 0x63, 0x73, 0x6e, 0x00, 0x00, 0x00, 0x00,
        /* 226 */ 0x00, 0x08, 0x1c, 0x36, 0x00, 0x3e, 0x60, 0x7e, 0x63, 0x63, 0x73, 0x6e, 0x00, 0x00, 0x00, 0x00,
        /* 227 */ 0x00, 0x00, 0x6e, 0x3b, 0x00, 0x3e, 0x60, 0x7e, 0x63, 0x63, 0x73, 0x6e, 0x00, 0x00, 0x00, 0x00,
        /* 228 */ 0x00, 0x00, 0x36, 0x36, 0x00, 0x3e, 0x60, 0x7e, 0x63, 0x63, 0x73, 0x6e, 0x00, 0x00, 0x00, 0x00,
        /* 229 */ 0x00, 0x1c, 0x36, 0x1c, 0x00, 0x3e, 0x60, 0x7e, 0x63, 0x63, 0x73, 0x6e, 0x00, 0x00, 0x00, 0x00,
        /* 230 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x6e, 0xdb, 0xd8, 0xfe, 0x1b, 0xdb, 0x76, 0x00, 0x00, 0x00, 0x00,
        /* 231 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x63, 0x03, 0x03, 0x03, 0x63, 0x3e, 0x18, 0x30, 0x1e, 0x00,
        /* 232 */ 0x00, 0x0c, 0x18, 0x30, 0x00, 0x3e, 0x63, 0x63, 0x7f, 0x03, 0x63, 0x3e, 0x00, 0x00, 0x00, 0x00,
        /* 233 */ 0x00, 0x30, 0x18, 0x0c, 0x00, 0x3e, 0x63, 0x63, 0x7f, 0x03, 0x63, 0x3e, 0x00, 0x00, 0x00, 0x00,
        /* 234 */ 0x00, 0x08, 0x1c, 0x36, 0x00, 0x3e, 0x63, 0x63, 0x7f, 0x03, 0x63, 0x3e, 0x00, 0x00, 0x00, 0x00,
        /* 235 */ 0x00, 0x00, 0x36, 0x36, 0x00, 0x3e, 0x63, 0x63, 0x7f, 0x03, 0x63, 0x3e, 0x00, 0x00, 0x00, 0x00,
        /* 236 */ 0x00, 0x06, 0x0c, 0x18, 0x00, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x38, 0x00, 0x00, 0x00, 0x00,
        /* 237 */ 0x00, 0x18, 0x0c, 0x06, 0x00, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x38, 0x00, 0x00, 0x00, 0x00,
        /* 238 */ 0x00, 0x08, 0x1c, 0x36, 0x00, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x38, 0x00, 0x00, 0x00, 0x00,
        /* 239 */ 0x00, 0x00, 0x36, 0x36, 0x00, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x38, 0x00, 0x00, 0x00, 0x00,
        /* 240 */ 0x00, 0x00, 0x2c, 0x18, 0x34, 0x60, 0x7c, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x00, 0x00, 0x00, 0x00,
        /* 241 */ 0x00, 0x00, 0x6e, 0x3b, 0x00, 0x3b, 0x67, 0x63, 0x63, 0x63, 0x63, 0x63, 0x00, 0x00, 0x00, 0x00,
        /* 242 */ 0x00, 0x06, 0x0c, 0x18, 0x00, 0x3e, 0x63, 0x63, 0x63, 0x63, 0x63, 0x3e, 0x00, 0x00, 0x00, 0x00,
        /* 243 */ 0x00, 0x30, 0x18, 0x0c, 0x00, 0x3e, 0x63, 0x63, 0x63, 0x63, 0x63, 0x3e, 0x00, 0x00, 0x00, 0x00,
        /* 244 */ 0x00, 0x08, 0x1c, 0x36, 0x00, 0x3e, 0x63, 0x63, 0x63, 0x63, 0x63, 0x3e, 0x00, 0x00, 0x00, 0x00,
        /* 245 */ 0x00, 0x00, 0x6e, 0x3b, 0x00, 0x3e, 0x63, 0x63, 0x63, 0x63, 0x63, 0x3e, 0x00, 0x00, 0x00, 0x00,
        /* 246 */ 0x00, 0x00, 0x36, 0x36, 0x00, 0x3e, 0x63, 0x63, 0x63, 0x63, 0x63, 0x3e, 0x00, 0x00, 0x00, 0x00,
        /* 247 */ 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x7e, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00,
        /* 248 */ 0x00, 0x00, 0x00, 0x00, 0x20, 0x3e, 0x73, 0x6b, 0x6b, 0x6b, 0x67, 0x3e, 0x02, 0x00, 0x00, 0x00,
        /* 249 */ 0x00, 0x06, 0x0c, 0x18, 0x00, 0x63, 0x63, 0x63, 0x63, 0x63, 0x73, 0x6e, 0x00, 0x00, 0x00, 0x00,
        /* 250 */ 0x00, 0x30, 0x18, 0x0c, 0x00, 0x63, 0x63, 0x63, 0x63, 0x63, 0x73, 0x6e, 0x00, 0x00, 0x00, 0x00,
        /* 251 */ 0x00, 0x08, 0x1c, 0x36, 0x00, 0x63, 0x63, 0x63, 0x63, 0x63, 0x73, 0x6e, 0x00, 0x00, 0x00, 0x00,
        /* 252 */ 0x00, 0x00, 0x36, 0x36, 0x00, 0x63, 0x63, 0x63, 0x63, 0x63, 0x73, 0x6e, 0x00, 0x00, 0x00, 0x00,
        /* 253 */ 0x00, 0x30, 0x18, 0x0c, 0x00, 0x63, 0x63, 0x36, 0x36, 0x1c, 0x1c, 0x0c, 0x0c, 0x06, 0x03, 0x00,
        /* 254 */ 0x00, 0x00, 0x0f, 0x06, 0x06, 0x3e, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3e, 0x06, 0x06, 0x0f, 0x00,
        /* 255 */ 0x00, 0x00, 0x36, 0x36, 0x00, 0x63, 0x63, 0x36, 0x36, 0x1c, 0x1c, 0x0c, 0x0c, 0x06, 0x03, 0x00 }
};

light_font_t*
create_light_font(uint8_t* font_data, uint8_t width, uint8_t height, uint32_t glyph_count, uint16_t bytes_per_glyph)
{
    light_font_t* ret;

    if (!font_data)
        return nullptr;

    ret = heap_allocate(glyph_count * bytes_per_glyph + sizeof(light_font_t));

    /* ENOMEM */
    if (!ret)
        return nullptr;

    ret->bytes_per_glyph = bytes_per_glyph;
    ret->glyph_count = glyph_count;
    ret->height = height;
    ret->width = width;

    memcpy(ret->data, font_data, glyph_count * bytes_per_glyph);

    return ret;
}

void destroy_light_font(light_font_t* font)
{
    /* Can't destroy the default font eh dummy */
    if (!font || font == &default_light_font)
        return;

    heap_free(font);
}

int lf_get_glyph_at(light_font_t* font, uint32_t index, light_glyph_t* out)
{
    if (!out)
        return -1;

    if (index >= font->glyph_count)
        return -2;

    out->parent_font = font;
    out->data = &font->data[index * font->bytes_per_glyph];

    return 0;
}

int lf_get_glyph_for_char(light_font_t* font, char c, light_glyph_t* out)
{
    return lf_get_glyph_at(font, c, out);
}
