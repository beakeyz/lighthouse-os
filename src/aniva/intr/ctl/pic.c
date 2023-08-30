#include "pic.h"
#include "intr/ctl/ctl.h"
#include "intr/interrupts.h"
#include "intr/ctl/ctl.h"
#include "libk/string.h"
#include <mem/heap.h>
#include <dev/debug/serial.h>
#include <libk/io.h>
#include <system/asm_specifics.h>
#include <libk/stddef.h>

void pic_enable(int_controller_t* ctl)
{
  PIC_t* pic = ctl->private;

  pic->m_pic1_line = in8(PIC1_DATA);
  pic->m_pic2_line = in8(PIC2_DATA);

  // cascade init =D
  out8(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
  PIC_WAIT();
  out8(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
  PIC_WAIT();

  /* Start PIC1 at idt entry 32 */
  out8(PIC1_DATA, 0x20);
  PIC_WAIT();
  /* Start PIC2 at idt entry 40 */
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

  out8(PIC1_DATA, pic->m_pic1_line);
  out8(PIC2_DATA, pic->m_pic2_line);
}



void pic_disable(int_controller_t* this) {

  // send funnie
  out8(PIC1_DATA, 0xFF);
  out8(PIC2_DATA, 0xFF);
}

void pic_eoi(uint8_t irq_num) {
  // irq from pic2 :clown:, so send for both
  if (irq_num >= 8)
    out8(PIC2_COMMAND, PIC_EOI_CODE);

  out8(PIC1_COMMAND, PIC_EOI_CODE);
}


PIC_t pic = {
  .m_pic1_line = 0xFF,
  .m_pic2_line = 0xFF,
};

static void pic_disable_vector(uint8_t vector) {

  // PIC 2
  if (vector & 8) {
    pic.m_pic2_line = in8(PIC2_DATA) | (1 << (vector & 7));
    out8(PIC2_DATA, pic.m_pic2_line);
  } else {
    // PIC 1
    pic.m_pic1_line = in8(PIC1_DATA) | (1 << vector);
    out8(PIC1_DATA, pic.m_pic1_line);
  }
}

static void pic_enable_vector(uint8_t vector) {

  // PIC 2
  if (vector & 8) {
    pic.m_pic2_line = in8(PIC2_DATA) & ~(1 << (vector & 7));
    out8(PIC2_DATA, pic.m_pic2_line);
  } else {
    // PIC 1
    pic.m_pic1_line = in8(PIC1_DATA) & ~(1 << vector);
    out8(PIC1_DATA, pic.m_pic1_line);
  }
}

int_controller_t pic_controller = {
  .ictl_disable_vec = pic_disable_vector,
  .ictl_enable_vec = pic_enable_vector,
  .ictl_disable = pic_disable,
  .ictl_enable = pic_enable,
  .ictl_eoi = pic_eoi,
  .type = I8259,
  .private = &pic,
};

int_controller_t* get_pic()
{
  return &pic_controller;
}
