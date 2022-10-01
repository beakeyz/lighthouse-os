#include "serial.h"
#include "arch/x86/interupts/control/pic.h"
#include <libk/io.h>
#include <libk/stddef.h>
#include <libk/string.h>

void init_serial() {

    out8(COM1 + 1, 0x00);
    out8(COM1 + 3, 0x80);
    out8(COM1 + 0, 0x02);
    out8(COM1 + 1, 0x00);
    out8(COM1 + 3, 0x03);
    out8(COM1 + 2, 0xC7);
    out8(COM1 + 4, 0x0B);

}

void putch(char c) {
    static int was_cr = false;
    
    while ((in8(COM1 + 5) & 0x20) == 0) {
        PIC_WAIT();
    }
    
    if (c == '\n' && !was_cr)
        out8(COM1, '\r');

    out8(COM1, c);

    was_cr = c == '\r';
}

void print(const char* str) {
    size_t x = 0;
    while (str[x] != '\0') {
        putch(str[x++]);
    }
}

void println(const char* str) {
    size_t x = 0;
    while (str[x] != '\0') {
        putch(str[x++]);
    }
    if (str[x-1] != '\n') {
        putch('\n');
    }
}

