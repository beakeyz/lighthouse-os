#include "dev/usb/spec.h"
#include "dev/usb/usb.h"
#include "dev/usb/xfer.h"
#include "drivers/usb/ehci/ehci_spec.h"
#include "ehci.h"
#include "libk/data/linkedlist.h"
#include "libk/flow/error.h"
#include "mem/heap.h"
#include "mem/kmem_manager.h"
#include "mem/zalloc.h"
#include "sync/mutex.h"

ehci_xfer_t* create_ehci_xfer(struct usb_xfer* xfer, ehci_qh_t* qh)
{
  ehci_xfer_t* e_xfer;

  e_xfer = kmalloc(sizeof(*e_xfer));

  if (!e_xfer)
    return nullptr;

  memset(e_xfer, 0, sizeof(*e_xfer));

  e_xfer->xfer = xfer;
  e_xfer->qh = qh;

  return e_xfer;
}

void destroy_ehci_xfer(ehci_xfer_t* xfer)
{
  kfree(xfer);
}

int ehci_enq_xfer(ehci_hcd_t* ehci, ehci_xfer_t* xfer)
{
  mutex_lock(ehci->transfer_lock);

  list_append(ehci->transfer_list, xfer);

  mutex_unlock(ehci->transfer_lock);
  return 0;
}

/*!
 * @brief: Remove a transfer from the ehci hcds queue
 */
int ehci_deq_xfer(ehci_hcd_t* ehci, ehci_xfer_t* xfer)
{
  bool remove_success;

  mutex_lock(ehci->transfer_lock);

  remove_success = list_remove_ex(ehci->transfer_list, xfer);

  mutex_unlock(ehci->transfer_lock);

  if (remove_success)
    return 0;

  return -1;
}

static inline int _ehci_qh_link_qtd(ehci_qh_t* qh, ehci_qtd_t* a, bool do_alt)
{
  a->prev = qh->qtd_last;

  if (!qh->qtd_last) {
    qh->qtd_link = a;
    qh->qtd_last = a;
    qh->hw_qtd_next = a->qtd_dma_addr;
    qh->hw_qtd_alt_next = EHCI_FLLP_TYPE_END;

    return 0;
  }

  qh->qtd_last->next = a;
  qh->qtd_last->hw_next = a->qtd_dma_addr;
  qh->qtd_last->alt_next = do_alt ? qh->qtd_alt : NULL;
  qh->qtd_last->hw_alt_next = do_alt ? qh->qtd_alt->qtd_dma_addr : EHCI_FLLP_TYPE_END;

  qh->qtd_last = a;
  return 0;
}

static inline void _ehci_qh_sanitize_queue(ehci_qh_t* qh)
{
  ehci_qtd_t* qtd;

  qtd = qh->qtd_link;

  if (!qtd)
    return;

  do {
    /* Make sure the end bit is cleared */
    if (qtd->alt_next)
      qtd->hw_alt_next &= ~EHCI_FLLP_TYPE_END;
    else
      qtd->hw_alt_next = EHCI_FLLP_TYPE_END;
    
    /* Make sure the end bit is cleared again */
    if (qtd->next)
      qtd->hw_next &= ~EHCI_FLLP_TYPE_END;
    else
      qtd->hw_next = EHCI_FLLP_TYPE_END;

    qtd = qtd->next;
  } while (qtd);
}

static inline void _try_allocate_qtd_buffer(ehci_hcd_t* ehci, ehci_qtd_t* qtd)
{
  paddr_t c_phys;

  if (!qtd->len)
    return;

  qtd->buffer = (void*)Must(__kmem_kernel_alloc_range(qtd->len, NULL, KMEM_FLAG_DMA));

  /* This allocation should be page aligned, so we're all good on that part */
  c_phys = kmem_to_phys(nullptr, (vaddr_t)qtd->buffer);

  for (uint32_t i = 0; i < 5; i++) {
    qtd->hw_buffer[i] = i ? (c_phys & EHCI_QTD_PAGE_MASK) : c_phys;
    qtd->hw_buffer_hi[i] = NULL;

    c_phys += SMALL_PAGE_SIZE;
  }
}

static ehci_qtd_t* _create_ehci_qtd_raw(ehci_hcd_t* ehci, size_t bsize, uint8_t pid)
{
  paddr_t this_dma;
  ehci_qtd_t* ret;

  ret = zalloc_fixed(ehci->qtd_pool);

  if (!ret)
    return nullptr;

  memset(ret, 0, sizeof(*ret));

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

void ehci_init_qh(ehci_qh_t* qh, usb_xfer_t* xfer)
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

  qh->hw_info_0 |= (
      EHCI_QH_DEVADDR(xfer->req_devaddr) |
      EHCI_QH_EP_NUM(xfer->req_endpoint) |
      EHCI_QH_MPL(8) |
      EHCI_QH_TOGGLE_CTL);
  qh->hw_info_1 = EHCI_QH_MULT_VAL(1);

  if(xfer->device->speed == USB_HIGHSPEED)
    return;

  qh->hw_info_0 |= (xfer->req_type == USB_CTL_XFER ? EHCI_QH_CONTROL_EP : 0);
  qh->hw_info_1 |= (
      EHCI_QH_HUBADDR(xfer->req_hubaddr) |
      EHCI_QH_HUBPORT(xfer->req_hubport));
}

ehci_qh_t* create_ehci_qh(ehci_hcd_t* ehci, struct usb_xfer* xfer)
{
  paddr_t this_dma;
  ehci_qh_t* qh;

  qh = zalloc_fixed(ehci->qh_pool);

  if (!qh)
    return nullptr;

  this_dma = kmem_to_phys(nullptr, (vaddr_t)qh);

  /* Sanity */
  ASSERT_MSG(ALIGN_UP((uint64_t)this_dma, 32) == (uint64_t)this_dma, "EHCI: Got an unaligned qh!");

  /* Zero the thing, just to be sure */
  memset(qh, 0, sizeof(*qh));

  /* Try to create the initial qtd for this qh */
  qh->qtd_alt = _create_ehci_qtd_raw(ehci, NULL, NULL);

  if (!qh->qtd_alt) {
    zfree_fixed(ehci->qh_pool, qh);
    return nullptr;
  }

  /* This is not an active qtd */
  qh->qtd_alt->hw_token &= ~EHCI_QTD_STATUS_ACTIVE;

  /* Hardware init for the qh */
  qh->hw_cur_qtd = NULL;
  qh->hw_next = EHCI_FLLP_TYPE_END;

  /* Hardware init for the qtd overlay thingy */
  qh->hw_qtd_next = qh->qtd_alt->qtd_dma_addr;
  qh->hw_qtd_alt_next = EHCI_FLLP_TYPE_END;

  /* Software init */
  qh->qh_dma = this_dma | EHCI_FLLP_TYPE_QH;
  qh->next = nullptr;
  qh->prev = nullptr;
  qh->qtd_link = nullptr;
  qh->qtd_last = nullptr;

  if (xfer)
    ehci_init_qh(qh, xfer);

  return qh;
}

void destroy_ehci_qh(ehci_hcd_t* ehci, ehci_qh_t* qh);

ehci_qtd_t* create_ehci_qtd(ehci_hcd_t* ehci, struct usb_xfer* xfer, ehci_qh_t* qh);

/*!
 * @brief: Write to a queue transfer chain
 *
 * @returns: The amount of bytes that have been written to the chain
 */ 
static size_t _ehci_write_qtd_chain(ehci_hcd_t* ehci, ehci_qtd_t* qtd, ehci_qtd_t** blast_qtd, void* buffer, size_t bsize)
{
  size_t written_size;
  size_t c_written_size;
  ehci_qtd_t* c_qtd;

  if (!ehci || !qtd)
    return NULL;

  c_qtd = qtd;
  written_size = NULL;

  do {
    c_written_size = bsize > qtd->len ?
      qtd->len :
      bsize;

    /* Copy the bytes */
    memcpy(qtd->buffer, buffer + written_size, c_written_size);

    /* Mark the written bytes */
    written_size += c_written_size;

    if (c_qtd->hw_next & EHCI_FLLP_TYPE_END)
      break;

    if (written_size >= bsize)
      break;

    c_qtd = c_qtd->next;
  } while(c_qtd);

  if (blast_qtd)
    *blast_qtd = c_qtd;

  return written_size;
}

int ehci_init_ctl_queue(ehci_hcd_t* ehci, struct usb_xfer* xfer, ehci_qh_t* qh)
{
  ehci_qtd_t* setup_desc, *stat_desc;
  usb_ctlreq_t* ctl;

  ctl = xfer->req_buffer;

  setup_desc = _create_ehci_qtd_raw(ehci, sizeof(*ctl), EHCI_QTD_PID_SETUP);
  stat_desc = _create_ehci_qtd_raw(ehci, 0, xfer->req_direction == USB_DIRECTION_DEVICE_TO_HOST ? EHCI_QTD_PID_IN : EHCI_QTD_PID_OUT);

  /* Try to write this bitch */
  if (_ehci_write_qtd_chain(ehci, setup_desc, NULL, ctl, sizeof(*ctl)) != sizeof(*ctl))
    goto dealloc_and_exit;

  /* Link the descriptors */
  _ehci_qh_link_qtd(qh, setup_desc, true);
  _ehci_qh_link_qtd(qh, stat_desc, true);

  /* Clean this bitch */
  _ehci_qh_sanitize_queue(qh);
  return 0;

dealloc_and_exit:
  kernel_panic("TODO: ehci_init_ctl_queue dealloc_and_exit");
  return -1;
}

int ehci_init_data_queue(ehci_hcd_t* ehci, struct usb_xfer* xfer, ehci_qh_t* qh)
{
  kernel_panic("TODO: ehci_init_data_queue");
}

