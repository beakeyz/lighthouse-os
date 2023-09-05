#ifndef __ANIVA_XHCI_HUB__
#define __ANIVA_XHCI_HUB__

/*
 * xHCI structures and global functions
 */

#include "libk/flow/doorbell.h"
#include "sync/mutex.h"
#include <libk/stddef.h>

struct usb_hub;

#define XHCI_PORT_REG_NUM 4

#define XHCI_PORTSC		0
#define XHCI_PORTPMSC	1
#define XHCI_PORTLI		2
#define XHCI_PORTHLPMC	3

typedef struct xhci_op_regs {
  uint32_t cmd;
  uint32_t status;
  uint32_t page_size;
  uint32_t res0[2];
  uint32_t device_notif;
  uintptr_t cmd_ring;
  uint32_t res1[4];
  uintptr_t dcbaa_ptr;
  uint32_t config_reg;
  uint32_t res2[241];
  uint32_t port_status_base;
  uint32_t port_pwr_base;
  uint32_t port_link_base;
  uint32_t res3;
  uint32_t res4[XHCI_PORT_REG_NUM*254];
} xhci_op_regs_t;

/*
 * Layout: Section 5.6 of the xhci spec
 */
typedef struct xhci_db_array {
  uint32_t db[256];
} xhci_db_array_t;

/*
 * xhci contexts
 */

/* NOTE: host controller might use 64-byte contex structures */
#define XHCI_CTX_BYTES 32

typedef struct xhci_slot_ctx {
  uint32_t dev_info;
  uint32_t dev_info2;
  uint32_t tt_info;
  uint32_t dev_state;
  uint32_t res[4];
} xhci_slot_ctx_t;

typedef struct xhci_endpoint_ctx {
  uint32_t ep_info;
  uint32_t ep_info2;
  uintptr_t deq_ptr;
  uint32_t tx_info;
  uint32_t res[3];
} xhci_endpoint_ctx_t;

typedef struct xhci_ip_ctl_ctx {
  uint32_t drp_flags;
  uint32_t add_flags;
  uint32_t res[6];
} xhci_ip_ctl_ctx_t;

/*
 * DMA supporting 'context'
 */
typedef struct xhci_dma_ctx {
  uint32_t type;
  uint32_t size;
  uint8_t* data;
  uintptr_t dma_addr;
} xhci_input_dma_t;

#define XHCI_DMA_CTX_DEVICE 0x01
#define XHCI_DMA_CTX_INPUT  0x02

/*
 * xhci command
 */

typedef struct xhci_cmd {
  xhci_input_dma_t* input_ctx;

  /* Store waiters for this command */
  kdoorbell_t* doorbell;

  /* TODO: */

} xhci_cmd_t;

typedef struct xhci_hub {
  struct usb_hub* parent;

  mutex_t* event_lock;

  uint32_t port_count;

  uintptr_t register_ptr;
  uint32_t cap_regs_offset;
  uint32_t oper_regs_offset;
  uint32_t runtime_regs_offset;
  uint32_t doorbell_regs_offset;
} xhci_hub_t;

/*
 * These macros are only safe to use after the xhci device has discovered all their offsets
 * we also assume 32-bit I/O so TODO: figure out if there are ever exceptions to this
 */
#define xhci_cap_read(hub, offset) ((hub)->parent->mmio_ops->mmio_read32((hub)->parent, (hub)->cap_regs_offset + (offset)))
#define xhci_cap_write(hub, offset, value) ((hub)->parent->mmio_ops->mmio_write32((hub)->parent, (hub)->cap_regs_offset + (offset), (value)))

#define xhci_oper_read(hub, offset) ((hub)->parent->mmio_ops->mmio_read32((hub)->parent, (hub)->oper_regs_offset + (offset)))
#define xhci_oper_write(hub, offset, value) ((hub)->parent->mmio_ops->mmio_write32((hub)->parent, (hub)->oper_regs_offset + (offset), (value)))

#define xhci_runtime_read(hub, offset) ((hub)->parent->mmio_ops->mmio_read32((hub)->parent, (hub)->runtime_regs_offset + (offset)))
#define xhci_runtime_write(hub, offset, value) ((hub)->parent->mmio_ops->mmio_write32((hub)->parent, (hub)->runtime_regs_offset + (offset), (value)))

#define xhci_doorbell_read(hub, offset) ((hub)->parent->mmio_ops->mmio_read32((hub)->parent, (hub)->doorbell_regs_offset + (offset)))
#define xhci_doorbell_write(hub, offset, value) ((hub)->parent->mmio_ops->mmio_write32((hub)->parent, (hub)->doorbell_regs_offset + (offset), (value)))

#endif // !__ANIVA_XHCI_HUB__
