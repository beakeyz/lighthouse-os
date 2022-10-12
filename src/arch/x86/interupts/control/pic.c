#include "pic.h"
#include "arch/x86/interupts/interupts.h"
#include <arch/x86/dev/debug/serial.h>
#include <libk/io.h>
#include <libk/stddef.h>

uint16_t _irq_mask_cache = 0xffff;

static void enable_vector(uint8_t vec) {
  disable_interupts();

  if (!(_irq_mask_cache & (1 << vec))) {
    return;
  }
  _irq_mask_cache &= ~(1 << vec);

  uint8_t imr;
  if (vec & 8) {
    imr = in8(PIC2_DATA);
    out8(PIC2_DATA, imr);
  } else {
    imr = in8(PIC1_DATA);
    out8(PIC1_DATA, imr);
  }

  enable_interupts();
}

void init_pic() {

  uint8_t mask1 = in8(PIC1_DATA);
  uint8_t mask2 = in8(PIC2_DATA);

  // cascade init =D
  out8(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
  PIC_WAIT();
  out8(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
  PIC_WAIT();

  out8(PIC1_DATA, 0x20);
  PIC_WAIT();
  out8(PIC2_DATA, 0x28);
  PIC_WAIT();

  out8(PIC1_DATA, 0x04);
  PIC_WAIT();
  out8(PIC2_DATA, 0x02);
  PIC_WAIT();

  out8(PIC1_DATA, 0x01);
  PIC_WAIT();
  out8(PIC2_DATA, 0x01);
  PIC_WAIT();

  out8(PIC1_DATA, mask1);
  out8(PIC2_DATA, mask2);

  out8(PIC1_DATA, 0b11111101);
  out8(PIC2_DATA, 0xFF);


  // disable_pic();
}

void disable_pic() {

  // send funnie
  out8(PIC1_DATA, 0xFF);
  out8(PIC2_DATA, 0xFF);

  enable_vector(2);
}

void pic_eoi(uint8_t num) {
  // irq from pic2 :clown:, so send for both
  if (num >= 8)
    out8(PIC2_COMMAND, PIC_EOI_CODE);

  out8(PIC1_COMMAND, PIC_EOI_CODE);
}
