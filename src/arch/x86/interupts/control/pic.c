#include "pic.h"
#include <arch/x86/dev/debug/serial.h>
#include <libc/io.h>
#include <libc/stddef.h>

void init_pic() {
    // cascade init =D
    out8(PIC1_COMMAND, ICW1_INIT|ICW1_ICW4); PIC_WAIT();
    out8(PIC2_COMMAND, ICW1_INIT|ICW1_ICW4); PIC_WAIT();

    out8(PIC1_DATA, 0x20); PIC_WAIT();
	out8(PIC2_DATA, 0x28); PIC_WAIT();

	out8(PIC1_DATA, 0x04); PIC_WAIT();
	out8(PIC2_DATA, 0x02); PIC_WAIT();

	out8(PIC1_DATA, 1); PIC_WAIT();
	out8(PIC2_DATA, 1); PIC_WAIT();

    disable_pic();
}

void disable_pic() {

    // send funnie
    out8(PIC1_DATA, 0xFF);
    out8(PIC2_DATA, 0xFF);
}

void pic_eoi(uint8_t num) {
    // irq from pic2 :clown:, so send for both
    if (num >= 8)
        out8(PIC2_COMMAND, PIC_EOI_CODE);

    out8(PIC1_COMMAND, PIC_EOI_CODE);
}
