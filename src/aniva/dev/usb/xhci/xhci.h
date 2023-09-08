#ifndef __ANIVA_XHCI_HUB__
#define __ANIVA_XHCI_HUB__

/*
 * xHCI structures and global functions
 */

#include "libk/flow/doorbell.h"
#include "sync/mutex.h"
#include <libk/stddef.h>

struct usb_hcd;

#define XHCI_PORT_REG_NUM 4

#define XHCI_PORTSC		0
#define XHCI_PORTPMSC	1
#define XHCI_PORTLI		2
#define XHCI_PORTHLPMC	3

#define XHCI_MAX_HC_SLOTS 256
#define XHCI_MAX_HC_PORTS 127

#define XHCI_HALT_TIMEOUT_US (32 * 1000)

typedef struct xhci_op_regs {
  uint32_t cmd;
  uint32_t status;
  uint32_t page_size;
  uint32_t res0[2];
  uint32_t device_notif;
  uintptr_t cmd_ring;
  uint32_t res1[4];
  uintptr_t dcbaa_ptr; // Device Context Base Address Array
  uint32_t config_reg;
  uint32_t res2[241];
  uint32_t port_status_base;
  uint32_t port_pwr_base;
  uint32_t port_link_base;
  uint32_t res3;
  uint32_t res4[XHCI_PORT_REG_NUM*254];
} xhci_op_regs_t;

/*
 * XHCI operation register command bits 
 * for the definitions, go to the linux kernel
 * xhci driver or the xhci spec
 */
#define XHCI_CMD_RUN (1 << 0)
#define XHCI_CMD_RESET (1 << 1)
#define XHCI_CMD_EIE (1 << 2)
#define XHCI_CMD_HSEIE (1 << 3)
/* bits 4 -> 6 are reserved */
#define XHCI_CMD_LRESET (1 << 7)
#define XHCI_CMD_HSS (1 << 8)
#define XHCI_CMD_HRS (1 << 9)
#define XHCI_CMD_EWE (1 << 10)
#define XHCI_CMD_PM_INDEX (1 << 11)
#define XHCI_CMD_ETE (1 << 14)

#define XHCI_IRQS (XHCI_CMD_EIE | XHCI_CMD_HSEIE | XHCI_CMD_EWE)

/*
 * XHCI operation register status bits
 */
#define XHCI_STS_HLT (1 << 0)
#define XHCI_STS_FATAL (1 << 2)
#define XHCI_STS_EINT (1 << 3)
#define XHCI_STS_PORT (1 << 4)
/* bits 5 -> 7 are reserved and zero */
#define XHCI_STS_SAVE (1 << 8)
#define XHCI_STS_CNR (1 << 11)
#define XHCI_STS_HCE (1 << 12)
/* bits 13 -> 31 are reserved and should not be touched */

/*
 * XHCI Port Status and Control Register bits 
 * op_regs->port_status_base
 */
#define XHCI_PORT_CONNECT (1 << 0)
#define XHCI_PORT_PE (1 << 1)
/* bit 2 reserved and zero */
#define XHCI_PORT_OC (1 << 3)
#define XHCI_PORT_RESET	(1 << 4)

/*
 * XHCI Port Link state Register bits
 * op_regs->port_link_base
 */
#define XHCI_PORT_PLS_MASK (0xf << 5)
#define XHCI_XDEV_U0 (0x0 << 5)
#define XHCI_XDEV_U1 (0x1 << 5)
#define XHCI_XDEV_U2 (0x2 << 5)
#define XHCI_XDEV_U3 (0x3 << 5)
#define XHCI_XDEV_DISABLED (0x4 << 5)
#define XHCI_XDEV_RXDETECT (0x5 << 5)
#define XHCI_XDEV_INACTIVE (0x6 << 5)
#define XHCI_XDEV_POLLING (0x7 << 5)
#define XHCI_XDEV_RECOVERY (0x8 << 5)
#define XHCI_XDEV_HOT_RESET (0x9 << 5)
#define XHCI_XDEV_COMP_MODE (0xa << 5)
#define XHCI_XDEV_TEST_MODE (0xb << 5)
#define XHCI_XDEV_RESUME (0xf << 5)

#define XHCI_PORT_POWR (1 << 9)

#define XHCI_DEV_SPEED_MASK	(0xf << 10)
#define XHCI_XDEV_FS (0x1 << 10)
#define XHCI_XDEV_LS (0x2 << 10)
#define XHCI_XDEV_HS (0x3 << 10)
#define XHCI_XDEV_SS (0x4 << 10)
#define XHCI_XDEV_SSP (0x5 << 10)

#define	XHCI_SLOT_SPEED_FS (XHCI_XDEV_FS << 10)
#define	XHCI_SLOT_SPEED_LS (XHCI_XDEV_LS << 10)
#define	XHCI_SLOT_SPEED_HS (XHCI_XDEV_HS << 10)
#define	XHCI_SLOT_SPEED_SS (XHCI_XDEV_SS << 10)
#define	XHCI_SLOT_SPEED_SSP (XHCI_XDEV_SSP << 10)

#define XHCI_PORT_CST (1 << 17)
#define XHCI_PORT_PEC (1 << 18)
#define XHCI_PORT_WRX (1 << 19)
#define XHCI_PORT_OCC (1 << 20)
#define XHCI_PORT_RC  (1 << 21)

typedef struct xhci_intr_regs {
  uint32_t irq_pending;
  uint32_t irq_ctl;
  uint32_t erst_size;
  uint32_t rsvd;
  uint64_t erst_base;
  uint64_t erst_dequeue;
} xhci_intr_regs_t;

typedef struct xhci_runtime_regs {
  uint32_t microframe_idx;
  uint32_t res[7];
  xhci_intr_regs_t ir_set[128];
} xhci_runtime_regs_t;

/*
 * XHCI capabilities registers
 *
 * These are at the direct start of the xhci MMIO config space
 */
typedef struct xhci_cap_regs {
  uint32_t hc_capbase;
  uint32_t hcs_params_1;
  uint32_t hcs_params_2;
  uint32_t hcs_params_3;
  uint32_t hcc_params_1;
  uint32_t db_arr_offset;
  uint32_t runtime_regs_offset;
  uint32_t hcc_params_2;
} xhci_cap_regs_t;

#define HC_LENGTH(cb) ((cb) & 0x00ff)
#define HC_VERSION(cb) (((cb) >> 16) & 0xffff)

#define HC_MAX_SLOTS(p) ((p) & 0xFF)
#define HC_MAX_INTER(p) (((p) >> 8) & 0x7FF)
#define HC_MAX_PORTS(p) (((p) >> 24) & 0x7F)

/* Weird define because there is a bitchin reserved bit right in the middle of our field =/ */
#define HC_MAX_SCRTCHPD(p) ((((p) >> 16) & 0x3e0) | (((p) >> 27) & 0x1f))

/*
 * Bits  0 -  7: Endpoint target
 * Bits  8 - 15: RsvdZ
 * Bits 16 - 31: Stream ID
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

typedef struct xhci_dev_ctx_array {
  uint64_t dev_ctx_ptrs[XHCI_MAX_HC_SLOTS];
  uint64_t dma;
} xhci_dev_ctx_array_t;

/*
 * xhci trb
 *
 * For the complete layout of every TRB type, check 
 * xhci spec Section 6.4
 */
typedef struct xhci_trb {
  uint64_t addr;
  uint32_t status;
  uint32_t control;
} xhci_trb_t;

/*
 * The upper most bits of the control field are reserved so we mask those
 * and the TRB type starts at the 10th bit in the field
 */
#define XHCI_TRB_TYPE_BM (0xfc00)
#define XHCI_TRB_TYPE(p) ((p) << 10)
#define XHCI_TRB_FIELD_TO_TYPE(p) (((p) & XHCI_TRB_TYPE_BM) >> 10)

/* bulk, interrupt, isoc scatter/gather, and control data stage */
#define TRB_NORMAL 1
/* setup stage for control transfers */
#define TRB_SETUP 2
/* data stage for control transfers */
#define TRB_DATA 3
/* status stage for control transfers */
#define TRB_STATUS 4
/* isoc transfers */
#define TRB_ISOC 5
/* TRB for linking ring segments */
#define TRB_LIN	6
#define TRB_EVENT_DATA 7
/* Transfer Ring No-op (not for the command ring) */
#define TRB_TR_NOOP	8
/* Command TRBs */
/* Enable Slot Command */
#define TRB_ENABLE_SLOT 9
/* Disable Slot Command */
#define TRB_DISABLE_SLOT 10
/* Address Device Command */
#define TRB_ADDR_DEV 11
/* Configure Endpoint Command */
#define TRB_CONFIG_EP 12
/* Evaluate Context Command */
#define TRB_EVAL_CONTEX 13
/* Reset Endpoint Command */
#define TRB_RESET_EP 14
/* Stop Transfer Ring Command */
#define TRB_STOP_RING 15
/* Set Transfer Ring Dequeue Pointer Command */
#define TRB_SET_DEQ	16
/* Reset Device Command */
#define TRB_RESET_DEV 17
/* Force Event Command (opt) */
#define TRB_FORCE_EVENT 18
/* Negotiate Bandwidth Command (opt) */
#define TRB_NEG_BANDWIDTH 19
/* Set Latency Tolerance Value Command (opt) */
#define TRB_SET_LT 20
/* Get port bandwidth Command */
#define TRB_GET_BW 21
/* Force Header Command - generate a transaction or link management packet */
#define TRB_FORCE_HEADER 22
/* No-op Command - not for transfer rings */
#define TRB_CMD_NOOP 23
/* TRB IDs 24-31 reserved */
/* Event TRBS */
/* Transfer Event */
#define TRB_TRANSFER 32
/* Command Completion Event */
#define TRB_COMPLETION 33
/* Port Status Change Event */
#define TRB_PORT_STATUS	34
/* Bandwidth Request Event (opt) */
#define TRB_BANDWIDTH_EVENT	35
/* Doorbell Event (opt) */
#define TRB_DOORBELL 36
/* Host Controller Event */
#define TRB_HC_EVENT 37
/* Device Notification Event - device sent function wake notification */
#define TRB_DEV_NOTE 38
/* MFINDEX Wrap Event - microframe counter wrapped */
#define TRB_MFINDEX_WRAP 39
/* TRB IDs 40-47 reserved, 48-63 is vendor-defined */
#define TRB_VENDOR_DEFINED_LOW 48
/* Nec vendor-specific command completion event. */
#define	TRB_NEC_CMD_COM	48
/* Get NEC firmware revision. */
#define	TRB_NEC_GET_FW 49

typedef struct xhci_segment {
  xhci_trb_t* trbs;
  struct xhci_segment* next;
  uint64_t dma;
  /* Linux has bounce buffers. WTF? */
} xhci_segment_t;

/*
 * xhci transfer descriptor
 */
typedef struct xhci_td {
  /* All the trbs in a td will be contiguous, so we only need one pointer */
  xhci_trb_t* trbs;

  uint32_t trb_count;
  uint32_t status;
} xhci_td_t;

#define XHCI_RING_TYPE_CTL 0
#define XHCI_RING_TYPE_ISOC 1
#define XHCI_RING_TYPE_BULC 2
#define XHCI_RING_TYPE_INTR 3
#define XHCI_RING_TYPE_STREAM 4
#define XHCI_RING_TYPE_CMD 5
#define XHCI_RING_TYPE_EVENT 6

/*
 * The xhci ring is a structure that can be found at 
 * transfer rings and such. We basically keep track of the 
 * start, end, enqueue and dequeue pointers
 */
typedef struct xhci_ring {
  xhci_trb_t* enqueue;
  xhci_trb_t* dequeue;

  uint32_t ring_size;
  uint32_t ring_type;
} xhci_ring_t;

typedef struct xhci_scratchpad {
  uint64_t* array;
  uint64_t dma;
  void** buffers;
} xhci_scratchpad_t;

typedef struct xhci_erst_entry {
  uint64_t addr;
  uint32_t size;
  uint32_t res;
} xhci_erst_entry_t;

typedef struct xhci_erst {
  xhci_erst_entry_t* entries;
  uint32_t entry_count;
  uint32_t erst_size;
  uint64_t dma;
} xhci_erst_t;

/*
 * xhci command
 */

typedef struct xhci_cmd {
  xhci_input_dma_t* input_ctx;

  uint32_t status;
  uint32_t slot_id;

  /* Store waiters for this command */
  kdoorbell_t* doorbell;

  /* Underlying TRB of the command */
  xhci_trb_t* cmd_trb;
} xhci_cmd_t;

/*
 * Flags to mark the state that the xhc is in
 */
#define XHC_FLAG_HALTED 0x00000001

typedef struct xhci_hcd {
  struct usb_hcd* parent;

  uint32_t xhc_flags;

  mutex_t* event_lock;

  uint8_t sbrn;
  uint16_t hci_version;
  uint8_t max_slots;
  uint16_t max_interrupters;
  uint8_t max_ports;
  uint8_t isoc_threshold;

  uintptr_t register_ptr;
  uint32_t cap_regs_offset;
  uint32_t oper_regs_offset;
  uint32_t runtime_regs_offset;
  uint32_t doorbell_regs_offset;

  xhci_cap_regs_t* cap_regs;
  xhci_op_regs_t* op_regs;
  xhci_runtime_regs_t* runtime_regs;
  xhci_db_array_t* db_arr;
} xhci_hcd_t;

#define XHCI_CAP_OF(xhci_hub) ((xhci_hub)->cap_regs_offset)
#define XHCI_RR_OF(xhci_hub) ((xhci_hub)->runtime_regs_offset)
#define XHCI_OP_OF(xhci_hub) ((xhci_hub)->oper_regs_offset)

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
