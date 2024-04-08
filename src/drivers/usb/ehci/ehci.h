#ifndef __ANIVA_USB_EHCI_HCD__
#define __ANIVA_USB_EHCI_HCD__

#include "mem/zalloc.h"
#include <libk/stddef.h>

struct usb_hcd;

#define EHCI_SPINUP_LIMIT 16

typedef struct ehci_hcd {
  struct usb_hcd* hcd;

  size_t register_size;
  void* capregs;
  void* opregs;

  uint32_t hcs_params;
  uint32_t hcc_params;
  uint32_t portcount;
  uint32_t cur_interrupt_state;

  /* Periodic table stuffskis */
  uint32_t periodic_size;
  uint32_t* periodic_table;
  paddr_t periodic_dma;

  /* DMA Pools for the EHCI datastructures */
  zone_allocator_t* qh_pool;
  zone_allocator_t* qtd_pool;
  zone_allocator_t* itd_pool;
  zone_allocator_t* sitd_pool;

  thread_t* interrupt_polling_thread;
} ehci_hcd_t;

#endif // !__ANIVA_USB_EHCI_HCD__
