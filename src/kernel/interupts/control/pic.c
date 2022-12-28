#include "pic.h"
#include "interupts/control/interrupt_control.h"
#include "kernel/interupts/interupts.h"
#include "mem/kmalloc.h"
#include <kernel/dev/debug/serial.h>
#include <libk/io.h>
#include <libk/stddef.h>

// TODO: make work
uint16_t _irq_mask_cache = 0xffff;

static void enable_vector(uint8_t vec) {
  disable_interupts();

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

PIC_t* init_pic() {

  PIC_t* ret = kmalloc(sizeof(PIC_t));

  ret->m_pic1_line = 0b11111101;
  ret->m_pic2_line = 0b11111111;

  ret->m_controller.m_type = I8259;
  ret->m_controller.fInterruptEOI = pic_eoi;
  ret->m_controller.fControllerInit = (INTERRUPT_CONTROLLER_INIT)init_pic;
  ret->m_controller.fControllerDisable = pic_disable;

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

  out8(PIC1_DATA, ret->m_pic1_line);
  out8(PIC2_DATA, ret->m_pic2_line);

  return ret;
}

void pic_disable(void* this) {
  // Lets hope
  PIC_t* _this = (PIC_t*)this;

  if (_this->m_controller.m_type != I8259) {
    return; // invalid lmao (TODO: error handle)
  }

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
