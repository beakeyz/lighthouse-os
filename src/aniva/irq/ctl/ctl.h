#ifndef __ANIVA_INTERRUPT_CONTROL__
#define __ANIVA_INTERRUPT_CONTROL__
#include "libk/data/linkedlist.h"
#include <libk/stddef.h>

typedef enum {
  LEAGACY_DUAL_PIC = 1,    /* INTEL DUAL PIC */
  APIC = 2, /* INTEL I/O APIC */ 
} INTERRUPT_CONTROLLER_TYPE;

// Represent the irq controller (PIC, IOAPIC, ect.)
typedef struct int_controller {
  INTERRUPT_CONTROLLER_TYPE type;

  void (*ictl_eoi)(uint8_t vec);
  void (*ictl_disable)(struct int_controller*);
  void (*ictl_enable)(struct int_controller*);

  void (*ictl_enable_vec)(uint16_t vec);
  void (*ictl_disable_vec)(uint16_t vec);

  void (*ictl_map_vector)(uint16_t vec);

  void (*ictl_reset)(struct int_controller*);

  void* private; // pointer to its parent (SHOULD NOT BE USED OFTEN)
} int_controller_t;

void init_intr_ctl();
int_controller_t* get_controller_for_int_number(uint16_t int_num);

#endif // !__ANIVA_INTERRUPT_CONTROL__
