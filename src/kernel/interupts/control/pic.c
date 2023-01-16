#include "pic.h"
#include "interupts/control/interrupt_control.h"
#include "kernel/interupts/interupts.h"
#include "libk/string.h"
#include "mem/kmalloc.h"
#include <kernel/dev/debug/serial.h>
#include <libk/io.h>
#include <libk/stddef.h>

static void pic_enable_vector(uint8_t vector) __attribute__((used));
static void pic_disable_vector(uint8_t vector) __attribute__((used));

PIC_t* init_pic() {

  PIC_t* ret = kmalloc(sizeof(PIC_t));
  ret->m_controller = kmalloc(sizeof(InterruptController_t));

  ret->m_controller->__parent = ret;
  ret->m_pic1_line = 0b11111111;
  ret->m_pic2_line = 0b11111111;

  ret->m_controller->m_type = I8259;
  ret->m_controller->fInterruptEOI = pic_eoi;
  ret->m_controller->fControllerInit = (INTERRUPT_CONTROLLER_INIT)init_pic;
  ret->m_controller->fControllerDisable = pic_disable;
  ret->m_controller->fControllerEnableVector = pic_enable_vector;
  ret->m_controller->fControllerDisableVector = pic_disable_vector;

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

  if (_this->m_controller->m_type != I8259) {
    return; // invalid lmao (TODO: error handle)
  }

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

static void pic_disable_vector(uint8_t vector) {
  disable_interupts();
  uint8_t pic_vector_line = 0;

  // PIC 2
  if (vector & 0x8) {
    pic_vector_line = in8(PIC2_DATA);
    pic_vector_line |= (1 << (vector & 7));
    out8(PIC2_DATA, pic_vector_line);
  } else {
    // PIC 1
    pic_vector_line = in8(PIC2_DATA);
    pic_vector_line |= (1 << vector);
    out8(PIC2_DATA, pic_vector_line);
  }

  enable_interupts();
}

static void pic_enable_vector(uint8_t vector) {
  disable_interupts();
  uint8_t pic_vector_line = 0;

  // PIC 2
  if (vector & 8) {
    pic_vector_line = in8(PIC2_DATA);
    pic_vector_line &= ~(1 << (vector & 7));
    out8(PIC2_DATA, pic_vector_line);
  } else {
    // PIC 1
    pic_vector_line = in8(PIC1_DATA) & ~(1 << vector);
    out8(PIC1_DATA, pic_vector_line);
  }
  enable_interupts();
}

