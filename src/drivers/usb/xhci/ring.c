#include "dev/usb/hcd.h"
#include "libk/flow/error.h"
#include "libk/io.h"
#include "logging/log.h"
#include "mem/kmem_manager.h"
#include "mem/zalloc.h"
#include "xhci.h"

static bool valid_ring_type(uint32_t type)
{
  return (type <= XHCI_RING_TYPE_MAX);
}

/*!
 * @brief Allocate the needed memory for a xhci ring
 *
 */
xhci_ring_t* create_xhci_ring(xhci_hcd_t* hcd, uint32_t trb_count, uint32_t type)
{
  xhci_trb_t* last;
  xhci_ring_t* ret;

  if (!valid_ring_type(type))
    return nullptr;

  ret = kzalloc(sizeof(*ret));

  if (!ret)
    return nullptr;

  memset(ret, 0, sizeof(*ret));

  ret->ring_type = type;
  ret->ring_size = ALIGN_UP(trb_count * sizeof(xhci_trb_t), SMALL_PAGE_SIZE);

  ret->ring_buffer = (void*)Must(__kmem_kernel_alloc_range(ret->ring_size, NULL, KMEM_FLAG_KERNEL | KMEM_FLAG_DMA));
  ret->ring_dma = kmem_to_phys(nullptr, (vaddr_t)ret->ring_buffer);

  memset(ret->ring_buffer, 0, ret->ring_size);

  ret->enqueue = ret->ring_buffer;
  ret->dequeue = ret->enqueue;

  last = (ret->ring_buffer + ret->ring_size) - sizeof(xhci_trb_t);

  if (type != XHCI_RING_TYPE_EVENT) {
    /* Link the last address back */
    last->addr = ret->ring_dma;
  }

  return ret;
}

void destroy_xhci_ring(xhci_ring_t* ring)
{
  Must(__kmem_dealloc(nullptr, nullptr, (vaddr_t)ring->ring_buffer, ring->ring_size));

  kzfree(ring, sizeof(xhci_ring_t));
}

int xhci_cmd_ring_enqueue(xhci_hcd_t* xhci, xhci_trb_t* trb)
{
  usb_hcd_t* hcd;
  xhci_ring_t* cmd_ring;

  hcd = xhci->parent;
  cmd_ring = xhci->cmd_ring_ptr;

  println("Enqueueing a XHCI command!");
  (void)hcd;
  (void)cmd_ring;

  return 0;
}

/*!
 * @brief Set the cmd ring register to our cmd ring
 *
 * TODO: implement the option for a multi-segment (cmd) ring
 */
int xhci_set_cmd_ring(xhci_hcd_t* hcd)
{
  uint64_t cmd_ring_reg;
  xhci_ring_t* cmd_ring;

  if (!hcd->cmd_ring_ptr)
    return -1;

  cmd_ring = hcd->cmd_ring_ptr;
  cmd_ring_reg = mmio_read_qword(&hcd->op_regs->cmd_ring);

  /* Set the DMA address of the command ring */
  cmd_ring_reg &= XHCI_CMD_RING_RSVD_BITS | (cmd_ring->ring_dma & (uint64_t) ~(XHCI_CMD_RING_RSVD_BITS));

  mmio_write_qword(&hcd->op_regs->cmd_ring, cmd_ring_reg);

  return 0;
}

xhci_interrupter_t* create_xhci_interrupter(struct xhci_hcd* xhci)
{
  xhci_interrupter_t* ret;
  xhci_erst_entry_t* current_entry;

  ret = kzalloc(sizeof(xhci_interrupter_t));

  if (!ret)
    return NULL;

  memset(ret, 0, sizeof(xhci_interrupter_t));

  ret->event_ring = create_xhci_ring(xhci, XHCI_MAX_EVENTS, XHCI_RING_TYPE_EVENT);

  if (!ret->event_ring) {
    kzfree(ret, sizeof(*ret));
    return nullptr;
  }

  /*
   * FIXME: this is VERY FUCKING wasteful
   * we are using an entire page here because 
   * we only use one segment...
   * TODO: we need to compute the count of segments in our event ring in order
   * to calculate the size of this erst. But since that count is always 1 for 
   * now, the size is going to be just the size of one erst entry =/
   */
  ret->erst.entry_count = 1;
  ret->erst.erst_size = ALIGN_UP(ret->erst.entry_count * sizeof(xhci_erst_entry_t), PAGE_SIZE);
  ret->erst.entries = (void*)Must(__kmem_kernel_alloc_range(ret->erst.erst_size, NULL, KMEM_FLAG_KERNEL | KMEM_FLAG_DMA));
  
  for (uint32_t i = 0; i < ret->erst.entry_count; i++) {
    current_entry = &ret->erst.entries[i];

    current_entry->addr = ret->event_ring->ring_dma;
    current_entry->size = XHCI_TRBS_PER_SEGMENT;
    current_entry->res = 0;
  }

  return ret;
}

void destory_xhci_interrupter(xhci_interrupter_t* interrupter)
{
  __kmem_dealloc(nullptr, nullptr, (vaddr_t)interrupter->erst.entries, interrupter->erst.erst_size);

  destroy_xhci_ring(interrupter->event_ring);

  kzfree(interrupter, sizeof(*interrupter));

  kernel_panic("TODO: clean the interrupter from the XHCI registers");
}

int xhci_add_interrupter(struct xhci_hcd* xhci, xhci_interrupter_t* intr, uint32_t num)
{
  if (num > xhci->max_interrupters)
    return -1;

  intr->ir_regs = &xhci->runtime_regs->ir_set[num];

  /* Set the erst size */
  uint32_t size = mmio_read_dword(&intr->ir_regs->erst_size);

  size &= XHCI_ERST_SIZE_MASK;
  size |= intr->erst.entry_count;

  mmio_write_dword(&intr->ir_regs->erst_size, size);

  /* Set the base */
  uint64_t ptr = mmio_read_qword(&intr->ir_regs->erst_base);

  ptr &= XHCI_ERST_PTR_MASK;
  ptr |= (intr->erst.dma & (uint64_t) ~(XHCI_ERST_PTR_MASK));

  mmio_write_qword(&intr->ir_regs->erst_base, ptr);

  /* Set the dequeue */
  uint64_t deq = mmio_read_qword(&intr->ir_regs->erst_dequeue);

  deq &= XHCI_ERST_PTR_MASK;
  deq |= (intr->event_ring->ring_dma & (uint64_t) ~(XHCI_ERST_PTR_MASK));

  mmio_write_qword(&intr->ir_regs->erst_dequeue, deq);

  return 0;
}
