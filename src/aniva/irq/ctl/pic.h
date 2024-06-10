/*
 * For legacy pic support
 */
#ifndef __C_PIC___
#define __C_PIC___

#include "irq/ctl/irqchip.h"
#include "libk/io.h"
#include <libk/stddef.h>

#define PIC_EOI_CODE 0x20

#define PIC1 0x20 /* IO base address for master PIC */
#define PIC2 0xA0 /* IO base address for slave PIC */
#define PIC1_COMMAND PIC1
#define PIC1_DATA (PIC1 + 1)
#define PIC2_COMMAND PIC2
#define PIC2_DATA (PIC2 + 1)

#define ICW1_ICW4 0x01
#define ICW1_INIT 0x10

typedef struct PIC {
    uint8_t m_pic1_line;
    uint8_t m_pic2_line;
} PIC_t;

irq_chip_t* get_pic();

#endif // !__C_PIC___
