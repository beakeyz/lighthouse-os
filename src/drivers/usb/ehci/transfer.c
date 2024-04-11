#include "dev/usb/usb.h"
#include "dev/usb/xfer.h"
#include "drivers/usb/ehci/ehci_spec.h"
#include "ehci.h"
#include "libk/flow/error.h"
#include "mem/kmem_manager.h"
#include "mem/zalloc.h"

static inline void _try_allocate_qtd_buffer(ehci_hcd_t* ehci, ehci_qtd_t* qtd)
{
  paddr_t c_phys;

  if (!qtd->len)
    return;

  qtd->buffer = (void*)Must(__kmem_kernel_alloc_range(qtd->len, NULL, KMEM_FLAG_DMA));

  /* This allocation should be page aligned, so we're all good on that part */
  c_phys = kmem_to_phys(nullptr, (vaddr_t)qtd->buffer);

  for (uint32_t i = 0; i < 5; i++) {
    qtd->hw_buffer[i] = c_phys;
    qtd->hw_buffer_hi[i] = NULL;

    c_phys += SMALL_PAGE_SIZE;
  }
}

static inline ehci_qtd_t* _create_ehci_qtd_raw(ehci_hcd_t* ehci, size_t bsize, uint8_t pid)
{
  paddr_t this_dma;
  ehci_qtd_t* ret;

  ret = zalloc_fixed(ehci->qtd_pool);

  if (!ret)
    return nullptr;

  this_dma = kmem_to_phys(nullptr, (vaddr_t)ret);

  ret->hw_next = EHCI_FLLP_TYPE_END;
  ret->hw_alt_next = EHCI_FLLP_TYPE_END;

  /* Token is kinda like a flags field */
  ret->hw_token = (bsize << EHCI_QTD_BYTES_SHIFT) |
    (3 << EHCI_QTD_ERRCOUNT_SHIFT) |
    (pid << EHCI_QTD_PID_SHIFT) |
    EHCI_QTD_STATUS_ACTIVE;

  ret->qtd_dma_addr = this_dma;
  ret->prev = nullptr;
  ret->next = nullptr;
  ret->buffer = nullptr;
  ret->len = bsize;

  _try_allocate_qtd_buffer(ehci, ret);

  return ret;
}

static inline void _init_qh(ehci_qh_t* qh, usb_xfer_t* xfer)
{
  size_t max_pckt_size;

  switch (xfer->device->speed) {
    case USB_LOWSPEED:
      qh->hw_info_0 = EHCI_QH_LOW_SPEED;
      break;
    case USB_FULLSPEED:
      qh->hw_info_0 = EHCI_QH_FULL_SPEED;
      break;
    case USB_HIGHSPEED:
      qh->hw_info_0 = EHCI_QH_HIGH_SPEED;
      break;
    default:
      /* This is bad lol */
      break;
  }

  (void)usb_xfer_get_max_packet_size(xfer, &max_pckt_size);

  //qh->hw_info_0 |= (max_pckt_size << EHCI_QH_)
  // ???????
}

ehci_qh_t* create_ehci_qh(ehci_hcd_t* ehci, struct usb_xfer* xfer)
{
  paddr_t this_dma;
  ehci_qtd_t* qtd;
  ehci_qh_t* qh;

  qh = zalloc_fixed(ehci->qh_pool);

  if (!qh)
    return nullptr;

  /* Sanity */
  ASSERT_MSG(ALIGN_UP((uint64_t)qh, 32) == (uint64_t)qh, "EHCI: Got an unaligned qh!");

  this_dma = kmem_to_phys(nullptr, (vaddr_t)qh);

  /* Zero the thing, just to be sure */
  memset(qh, 0, sizeof(*qh));

  /* Try to create the initial qtd for this qh */
  qtd = _create_ehci_qtd_raw(ehci, NULL, NULL);

  if (!qtd) {
    zfree_fixed(ehci->qh_pool, qh);
    return nullptr;
  }

  /* Hardware init for the qh */
  qh->hw_cur = this_dma | EHCI_FLLP_TYPE_QH;
  qh->hw_next = EHCI_FLLP_TYPE_END;

  /* Hardware init for the qtd overlay thingy */
  qh->hw_qtd_next = qtd->qtd_dma_addr;
  qh->hw_qtd_alt_next = EHCI_FLLP_TYPE_END;

  /* Software init */
  qh->next = nullptr;
  qh->prev = nullptr;
  qh->qh_dma = this_dma;
  qh->qtd_link = qtd;

  _init_qh(qh, xfer);

  return qh;
}

void destroy_ehci_qh(ehci_hcd_t* ehci, ehci_qh_t* qh);

ehci_qtd_t* create_ehci_qtd(ehci_hcd_t* ehci, struct usb_xfer* xfer, ehci_qh_t* qh);
