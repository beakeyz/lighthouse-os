/*
* For legacy pic support
*/
#ifndef __C_PIC___
#define __C_PIC___

#include "intr/ctl/ctl.h"
#include "libk/io.h"
#include <libk/stddef.h>

#define PIC_EOI_CODE    0x20

#define PIC1		0x20		/* IO base address for master PIC */
#define PIC2		0xA0		/* IO base address for slave PIC */
#define PIC1_COMMAND	PIC1
#define PIC1_DATA	(PIC1+1)
#define PIC2_COMMAND	PIC2
#define PIC2_DATA	(PIC2+1)

#define ICW1_ICW4       0x01
#define ICW1_INIT       0x10

typedef struct PIC {
  uint8_t m_pic1_line;
  uint8_t m_pic2_line;
} PIC_t;

static inline void out8_pic(uint16_t port, uint8_t value)
{
  out8(port, value);
  delay(2);
}

int_controller_t* get_pic();

void pic_enable(int_controller_t* ctl);
void pic_disable(int_controller_t* ctl);

void pic_eoi(uint8_t num);

#endif // !__C_PIC___