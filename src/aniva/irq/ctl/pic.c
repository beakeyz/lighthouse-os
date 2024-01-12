#include "pic.h"
#include "irq/ctl/ctl.h"
#include "irq/interrupts.h"
#include "irq/ctl/ctl.h"
#include "libk/string.h"
#include "logging/log.h"
#include <mem/heap.h>
#include <dev/debug/serial.h>
#include <libk/io.h>
#include <system/asm_specifics.h>
#include <libk/stddef.h>

static inline void out8_pic(uint16_t port, uint8_t value)
{
  out8(port, value);
  udelay(2);
}

void pic_enable(irq_chip_t* ctl)
{
  PIC_t* pic = ctl->private;

  pic->m_pic1_line = in8(PIC1_DATA);
  pic->m_pic2_line = in8(PIC2_DATA);

  // cascade init =D
  out8_pic(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
  out8_pic(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);

  /* Start PIC1 at idt entry 32 */
  out8_pic(PIC1_DATA, 0x20);
  /* Start PIC2 at idt entry 40 */
  out8_pic(PIC2_DATA, 0x28);

  out8_pic(PIC1_DATA, 0x04);
  out8_pic(PIC2_DATA, 0x02);

  out8_pic(PIC1_DATA, 0x01);
  out8_pic(PIC2_DATA, 0x01);

  out8(PIC1_DATA, pic->m_pic1_line);
  out8(PIC2_DATA, pic->m_pic2_line);
}



void pic_disable(irq_chip_t* this) {

  // send funnie
  out8(PIC1_DATA, 0xFF);
  out8(PIC2_DATA, 0xFF);
}

void pic_eoi(irq_chip_t* chip, uint32_t irq_num) 
{
  // irq from pic2 :clown:, so send for both
  if (irq_num >= 8)
    out8(PIC2_COMMAND, PIC_EOI_CODE);

  out8(PIC1_COMMAND, PIC_EOI_CODE);
}


PIC_t pic = {
  .m_pic1_line = 0xFF,
  .m_pic2_line = 0xFF,
};

static int pic_disable_vector(irq_chip_t* chip, uint32_t vector) 
{
  // PIC 2
  if (vector & 8) {
    pic.m_pic2_line = in8(PIC2_DATA) | (1 << (vector & 7));
    out8(PIC2_DATA, pic.m_pic2_line);
  } else {
    // PIC 1
    pic.m_pic1_line = in8(PIC1_DATA) | (1 << vector);
    out8(PIC1_DATA, pic.m_pic1_line);
  }

  return 0;
}

static int pic_enable_vector(irq_chip_t* chip, uint32_t vector) 
{
  // PIC 2
  if (vector & 8) {
    pic.m_pic2_line = in8(PIC2_DATA) & ~(1 << (vector & 7));
    out8(PIC2_DATA, pic.m_pic2_line);
  } else {
    // PIC 1
    pic.m_pic1_line = in8(PIC1_DATA) & ~(1 << vector);
    out8(PIC1_DATA, pic.m_pic1_line);
  }

  return 0;
}

/*!
 * @brief: Mask all the vectors
 *
 * PIC only supports 16 active vectors at a time, so this is very easy
 */
static int pic_mask_all(irq_chip_t* chip)
{
  for (uint32_t i = 0; i < 16; i++) 
    pic_disable_vector(chip, i);

  return 0;
}

irq_chip_ops_t _pic_ops = {
  .f_mask_vec = pic_disable_vector,
  .f_unmask_vec = pic_enable_vector,
  .ictl_disable = pic_disable,
  .ictl_enable = pic_enable,
  .ictl_eoi = pic_eoi,
  .f_mask_all = pic_mask_all,
};

irq_chip_t pic_controller = {
  .ops = &_pic_ops,
  .private = &pic,
};

irq_chip_t* get_pic()
{
  return &pic_controller;
}
