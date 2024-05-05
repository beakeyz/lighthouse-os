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

static inline void _try_allocate_qtd_buffer(ehci_hcd_t* ehci, ehci_qtd_t* qtd)
{
  paddr_t c_phys;

  if (!qtd->len)
    return;

  qtd->buffer = (void*)Must(__kmem_kernel_alloc_range(qtd->len, NULL, KMEM_FLAG_DMA));

  memset(qtd->buffer, 0, qtd->len);

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
      EHCI_QH_MPL(max_pckt_size) |
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
  ehci_qtd_t* qtd;
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
  qtd = _create_ehci_qtd_raw(ehci, NULL, NULL);

  if (!qtd) {
    zfree_fixed(ehci->qh_pool, qh);
    return nullptr;
  }

  /* This is not an active qtd */
  qtd->hw_token &= ~EHCI_QTD_STATUS_ACTIVE;

  /* Hardware init for the qh */
  qh->hw_cur_qtd = NULL;
  qh->hw_next = EHCI_FLLP_TYPE_END;

  /* Hardware init for the qtd overlay thingy */
  qh->hw_qtd_next = qtd->qtd_dma_addr;
  qh->hw_qtd_alt_next = EHCI_FLLP_TYPE_END;

  /* Software init */
  qh->qh_dma = this_dma | EHCI_FLLP_TYPE_QH;
  qh->next = nullptr;
  qh->prev = nullptr;
  qh->qtd_link = qtd;
  qh->qtd_alt = qtd;

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

static void _ehci_link_qtds(ehci_qtd_t* a, ehci_qtd_t* b, ehci_qtd_t* alt)
{
  a->next = b;
  a->hw_next = b->qtd_dma_addr;

  printf("Linked qtd!\n");

  /* Set alt descriptor */
  a->alt_next =     (alt ? alt : NULL);
  a->hw_alt_next =  (alt ? alt->qtd_dma_addr : EHCI_FLLP_TYPE_END);
}

static void _ehci_xfer_attach_data_qtds(ehci_hcd_t* ehci, size_t bsize, enum USB_XFER_DIRECTION direction, ehci_qtd_t** start, ehci_qtd_t** end)
{
  // Bind more descriptors to include the data
  ehci_qtd_t* b = _create_ehci_qtd_raw(ehci, bsize, direction == USB_DIRECTION_DEVICE_TO_HOST ? EHCI_QTD_PID_IN : EHCI_QTD_PID_OUT);

  b->hw_token |= EHCI_QTD_DATA_TOGGLE;

  *start = b;
  *end = b;
}

int ehci_init_ctl_queue(ehci_hcd_t* ehci, struct usb_xfer* xfer, ehci_qh_t* qh)
{
  ehci_qtd_t* setup_desc, *stat_desc;
  ehci_qtd_t* data_start, *data_end;
  usb_ctlreq_t* ctl;

  ctl = xfer->req_buffer;

  setup_desc = _create_ehci_qtd_raw(ehci, sizeof(*ctl), EHCI_QTD_PID_SETUP);
  stat_desc = _create_ehci_qtd_raw(ehci, 0, xfer->req_direction == USB_DIRECTION_DEVICE_TO_HOST ? EHCI_QTD_PID_OUT : EHCI_QTD_PID_IN);

  data_start = setup_desc;
  data_end = setup_desc;

  /* Try to write this bitch */
  if (_ehci_write_qtd_chain(ehci, setup_desc, NULL, ctl, sizeof(*ctl)) != sizeof(*ctl))
    goto dealloc_and_exit;

  stat_desc->hw_token |= EHCI_QTD_IOC | EHCI_QTD_DATA_TOGGLE;

  if (xfer->resp_buffer)
    _ehci_xfer_attach_data_qtds(ehci, xfer->resp_size, xfer->req_direction, &data_start, &data_end);

  if (data_start != setup_desc)
    _ehci_link_qtds(setup_desc, data_start, qh->qtd_alt);

  _ehci_link_qtds(data_end, stat_desc, qh->qtd_alt);

  printf("Did setup!\n");
  qh->qtd_link = setup_desc;
  qh->hw_qtd_next = setup_desc->qtd_dma_addr;
  qh->hw_qtd_alt_next = EHCI_FLLP_TYPE_END;
  return 0;

dealloc_and_exit:
  kernel_panic("TODO: ehci_init_ctl_queue dealloc_and_exit");
  return -1;
}

int ehci_init_data_queue(ehci_hcd_t* ehci, struct usb_xfer* xfer, ehci_qh_t* qh)
{
  kernel_panic("TODO: ehci_init_data_queue");
}

/*!
 * @brief: Called right before the usb_xfer is marked completed
 */
int ehci_xfer_finalise(ehci_hcd_t* ehci, ehci_xfer_t* xfer)
{
  ehci_qtd_t* c_qtd;
  usb_xfer_t* usb_xfer;

  usb_xfer = xfer->xfer;

  switch (usb_xfer->req_direction) {
    /* Device might have given us info, populate the response buffer */
    case USB_DIRECTION_DEVICE_TO_HOST:
      c_qtd = xfer->qh->qtd_link;
      
      do {

        if (c_qtd->buffer) {
          usb_device_descriptor_t* desc = c_qtd->buffer;

          printf("desc len: %d\n", desc->length);
          printf("desc type: %d\n", desc->type);

          kernel_panic("Got a buffer??");
        }

        c_qtd = c_qtd->next;
      } while (c_qtd);

      break;
    /* We might have info to the device, check status */
    case USB_DIRECTION_HOST_TO_DEVICE:
      break;
  }
  return 0;
}
