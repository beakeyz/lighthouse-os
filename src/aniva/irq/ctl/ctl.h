#ifndef __ANIVA_INTERRUPT_CONTROL__
#define __ANIVA_INTERRUPT_CONTROL__

#include <libk/stddef.h>

struct irq;
struct irq_chip;

typedef struct irq_chip_ops {
  /* Acknowledge an IRQ */
  void (*ictl_eoi)(struct irq_chip*, uint32_t vec);

  /* Enable/disable the chip (This is handy when we switch from the legacy PIC to the APIC) */
  void (*ictl_disable)(struct irq_chip*);
  void (*ictl_enable)(struct irq_chip*);

  /* Mask vectors */
  int (*f_unmask_vec)(struct irq_chip*, uint32_t vec);
  int (*f_mask_vec)(struct irq_chip*, uint32_t vec);
  int (*f_mask_all)(struct irq_chip*);

  /* ??? */
  void (*ictl_map_vector)(struct irq_chip*, uint16_t vec);

  /* Quick reset of the management hardware */
  void (*ictl_reset)(struct irq_chip*);
} irq_chip_ops_t;

/*
 * Basic representation of a interrupt controller
 * i.e. PIC, APIC, ect.
 * 
 * In the case of most X86 systems we pretty much always deal with PIC (emulation) and APIC
 */
typedef struct irq_chip {
  irq_chip_ops_t* ops;
  /* Implemented by the anonymous chip drivers */
  void* private;
} irq_chip_t;

void init_intr_ctl();

int get_active_irq_chip(irq_chip_t** chip);

int irq_chip_ack(irq_chip_t* chip, struct irq* irq);
int irq_chip_enable(irq_chip_t* chip);
int irq_chip_disable(irq_chip_t* chip);

int irq_chip_mask(irq_chip_t* chip, uint32_t vec);
int irq_chip_unmask(irq_chip_t* chip, uint32_t vec);
int irq_chip_mask_all(irq_chip_t* chip);

#endif // !__ANIVA_INTERRUPT_CONTROL__
