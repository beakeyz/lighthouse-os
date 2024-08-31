#ifndef __ANIVA_XHCI_HUB__
#define __ANIVA_XHCI_HUB__

/*
 * xHCI structures and global functions
 */

#include "dev/usb/hcd.h"
#include "dev/usb/spec.h"
#include "dev/usb/usb.h"
#include "libk/flow/doorbell.h"
#include "mem/zalloc/zalloc.h"
#include "proc/thread.h"
#include "sync/mutex.h"
#include <libk/stddef.h>

struct usb_hcd;
struct usb_device;
struct usb_port;
struct xhci_hcd;
struct xhci_hub;

#define XHCI_PORT_REG_NUM 4

#define XHCI_PORTSC 0
#define XHCI_PORTPMSC 1
#define XHCI_PORTLI 2
#define XHCI_PORTHLPMC 3

#define XHCI_MAX_HC_SLOTS 255
#define XHCI_MAX_HC_PORTS 127

#define XHCI_MAX_EVENTS (16 * 13)
#define XHCI_MAX_COMMANDS (16)
#define XHCI_MAX_EPS (32)

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
    union {
        struct {
            uint32_t port_status_base;
            uint32_t port_pwr_base;
            uint32_t port_link_base;
            uint32_t res3;
        } ports[255];
        uint32_t res4[XHCI_PORT_REG_NUM * 255];
    };
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
#define XHCI_PORT_RO ((1 << 0) | (1 << 3) | (0xf << 10) | (1 << 30))
#define XHCI_PORT_RWS ((0xf << 5) | (1 << 9) | (0x3 << 14) | (0x7 << 25))

#define XHCI_PORT_STATUS_MASK 0x80FF00F7U

/* Current connect status */
#define XHCI_PORT_CCS (1 << 0)
/* Port enabled/disabled */
#define XHCI_PORT_PED (1 << 1)
/* bit 2 reserved and zero */
/* Over-current active */
#define XHCI_PORT_OCA (1 << 3)
/* Port reset */
#define XHCI_PORT_RESET (1 << 4)
/* R/W port link state */
#define XHCI_PORT_LINK_STATE_OFFSET 5
#define XHCI_PORT_LINK_STATE_MASK 0x0f
/* Port power status */
#define XHCI_PORT_POWR (1 << 9)

/* Port speed */
#define XHCI_PORT_SPEED_OFFSET 10
#define XHCI_PORT_SPEED_MASK 0x0f

/* Link state write strobe */
#define XHCI_PORT_LWS (1 << 16)
/* Connect status change */
#define XHCI_PORT_CSC (1 << 17)
/* Port Enable/Disable state change */
#define XHCI_PORT_PEC (1 << 18)
/* Port warm reset change */
#define XHCI_PORT_WRC (1 << 19)
/* Port Over-current state change */
#define XHCI_PORT_OCC (1 << 20)
/* Port reset change */
#define XHCI_PORT_RC (1 << 21)
/* Port link state change */
#define XHCI_PORT_PLC (1 << 22)
/* Port config error change */
#define XHCI_PORT_CEC (1 << 23)
/* Port cold attach status */
#define XHCI_PORT_CAS (1 << 24)
/* Port wake on connect enable */
#define XHCI_PORT_WCE (1 << 25)
/* Port wake on disconnect enable */
#define XHCI_PORT_WDE (1 << 26)
/* Port wake on over-current enable */
#define XHCI_PORT_WOE (1 << 27)
/* Port device removable status */
#define XHCI_PORT_DR (1 << 30)
/* Port warm reset */
#define XHCI_PORT_WPR (1 << 31)

#define XHCI_CMD_RING_PAUSED (1 << 1)
#define XHCI_CMD_RING_ABORT (1 << 2)
#define XHCI_CMD_RING_RUNNING (1 << 3)
#define XHCI_CMD_RING_RSVD_BITS (0x3f)

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

#define XHCI_DEV_SPEED_MASK (0xf << 10)
#define XHCI_XDEV_FS (0x1 << 10)
#define XHCI_XDEV_LS (0x2 << 10)
#define XHCI_XDEV_HS (0x3 << 10)
#define XHCI_XDEV_SS (0x4 << 10)
#define XHCI_XDEV_SSP (0x5 << 10)

#define XHCI_SLOT_SPEED_FS (XHCI_XDEV_FS << 10)
#define XHCI_SLOT_SPEED_LS (XHCI_XDEV_LS << 10)
#define XHCI_SLOT_SPEED_HS (XHCI_XDEV_HS << 10)
#define XHCI_SLOT_SPEED_SS (XHCI_XDEV_SS << 10)
#define XHCI_SLOT_SPEED_SSP (XHCI_XDEV_SSP << 10)

typedef struct xhci_intr_regs {
    uint32_t irq_pending;
    uint32_t irq_ctl;
    uint32_t erst_size;
    uint32_t rsvd;
    uint64_t erst_base;
    uint64_t erst_dequeue;
} xhci_intr_regs_t;

#define XHCI_ERST_SIZE_MASK (0xFFFF << 16)
#define XHCI_ERST_PTR_MASK (0xf)

#define XHCI_ER_IRQ_PENDING(p) ((p) & 0x1)
/* Idk man */
#define XHCI_ER_IRQ_CLEAR(p) ((p) & 0xfffffffe)
#define XHCI_ER_IRQ_ENABLE(p) ((XHCI_ER_IRQ_CLEAR(p)) | 0x2)
#define XHCI_ER_IRQ_DISABLE(p) ((XHCI_ER_IRQ_CLEAR(p)) & ~(0x2))

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

#define HC_LENGTH(cb) (((cb) >> 00) & 0x00ff)
#define HC_VERSION(cb) (((cb) >> 16) & 0xffff)

#define HC_MAX_SLOTS(p) ((p) & 0xFF)
#define HC_MAX_INTER(p) (((p) >> 8) & 0x7FF)
#define HC_MAX_PORTS(p) (((p) >> 24) & 0xFF)

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

#define DB_VALUE(ep, stream) ((((ep) + 1) & 0xff) | ((stream) << 16))

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

#define SLOT_0_ROUTE(x) ((x) & 0xFFFFF)
#define SLOT_0_ROUTE_GET(x) ((x) & 0xFFFFF)
#define SLOT_0_SPEED(x) (((x) & 0xF) << 20)
#define SLOT_0_SPEED_GET(x) (((x) >> 20) & 0xF)
#define SLOT_0_MTT_BIT (1U << 25)
#define SLOT_0_HUB_BIT (1U << 26)
#define SLOT_0_NUM_ENTRIES(x) (((x) & 0x1F) << 27)
#define SLOT_0_NUM_ENTRIES_GET(x) (((x) >> 27) & 0x1F)

#define SLOT_1_MAX_EXIT_LATENCY(x) ((x) & 0xFFFF)
#define SLOT_1_MAX_EXIT_LATENCY_GET(x) ((x) & 0xFFFF)
#define SLOT_1_RH_PORT(x) (((x) & 0xFF) << 16)
#define SLOT_1_RH_PORT_GET(x) (((x) >> 16) & 0xFF)
#define SLOT_1_NUM_PORTS(x) (((x) & 0xFF) << 24)
#define SLOT_1_NUM_PORTS_GET(x) (((x) >> 24) & 0xFF)

#define SLOT_2_TT_HUB_SLOT(x) ((x) & 0xFF)
#define SLOT_2_TT_HUB_SLOT_GET(x) ((x) & 0xFF)
#define SLOT_2_PORT_NUM(x) (((x) & 0xFF) << 8)
#define SLOT_2_PORT_NUM_GET(x) (((x) >> 8) & 0xFF)
#define SLOT_2_TT_TIME(x) (((x) & 0x3) << 16)
#define SLOT_2_TT_TIME_GET(x) (((x) >> 16) & 0x3)
#define SLOT_2_IRQ_TARGET(x) (((x) & 0x7F) << 22)
#define SLOT_2_IRQ_TARGET_GET(x) (((x) >> 22) & 0x7F)

#define SLOT_3_DEVICE_ADDRESS(x) ((x) & 0xFF)
#define SLOT_3_DEVICE_ADDRESS_GET(x) ((x) & 0xFF)
#define SLOT_3_SLOT_STATE(x) (((x) & 0x1F) << 27)
#define SLOT_3_SLOT_STATE_GET(x) (((x) >> 27) & 0x1F)

#define HUB_TTT_GET(x) (((x) >> 5) & 0x3)

typedef struct xhci_endpoint_ctx {
    uint32_t ep_info;
    uint32_t ep_info2;
    uintptr_t deq_ptr;
    uint32_t tx_info;
    uint32_t res[3];
} xhci_endpoint_ctx_t;

#define ENDPOINT_0_STATE(x) ((x) & 0x7)
#define ENDPOINT_0_STATE_GET(x) ((x) & 0x7)
#define ENDPOINT_0_MULT(x) (((x) & 0x3) << 8)
#define ENDPOINT_0_MULT_GET(x) (((x) >> 8) & 0x3)
#define ENDPOINT_0_MAXPSTREAMS(x) (((x) & 0x1F) << 10)
#define ENDPOINT_0_MAXPSTREAMS_GET(x) (((x) >> 10) & 0x1F)
#define ENDPOINT_0_LSA_BIT (1U << 15)
#define ENDPOINT_0_INTERVAL(x) (((x) & 0xFF) << 16)
#define ENDPOINT_0_INTERVAL_GET(x) (((x) >> 16) & 0xFF)

#define ENDPOINT_1_CERR(x) (((x) & 0x3) << 1)
#define ENDPOINT_1_CERR_GET(x) (((x) >> 1) & 0x3)
#define ENDPOINT_1_EPTYPE(x) (((x) & 0x7) << 3)
#define ENDPOINT_1_EPTYPE_GET(x) (((x) >> 3) & 0x7)
#define ENDPOINT_1_HID_BIT (1U << 7)
#define ENDPOINT_1_MAXBURST(x) (((x) & 0xFF) << 8)
#define ENDPOINT_1_MAXBURST_GET(x) (((x) >> 8) & 0xFF)
#define ENDPOINT_1_MAXPACKETSIZE(x) (((x) & 0xFFFF) << 16)
#define ENDPOINT_1_MAXPACKETSIZE_GET(x) (((x) >> 16) & 0xFFFF)

#define ENDPOINT_2_DCS_BIT (1U << 0)

#define ENDPOINT_4_AVGTRBLENGTH(x) ((x) & 0xFFFF)
#define ENDPOINT_4_AVGTRBLENGTH_GET(x) ((x) & 0xFFFF)
#define ENDPOINT_4_MAXESITPAYLOAD(x) (((x) & 0xFFFF) << 16)
#define ENDPOINT_4_MAXESITPAYLOAD_GET(x) (((x) >> 16) & 0xFFFF)

#define ENDPOINT_STATE_DISABLED 0
#define ENDPOINT_STATE_RUNNING 1
#define ENDPOINT_STATE_HALTED 2
#define ENDPOINT_STATE_STOPPED 3
#define ENDPOINT_STATE_ERROR 4
#define ENDPOINT_STATE_RESERVED_5 5
#define ENDPOINT_STATE_RESERVED_6 6
#define ENDPOINT_STATE_RESERVED_7 7

typedef struct xhci_ip_ctl_ctx {
    uint32_t drp_flags;
    uint32_t add_flags;
    uint32_t res[6];
} xhci_ip_ctl_ctx_t;

typedef struct xhci_device_ctx {
    xhci_slot_ctx_t slot_ctx;
    xhci_endpoint_ctx_t eps_ctx[XHCI_MAX_EPS - 1];
} xhci_device_ctx_t;

typedef struct xhci_input_device_ctx {
    xhci_ip_ctl_ctx_t input;
    xhci_slot_ctx_t slot_ctx;
    xhci_endpoint_ctx_t eps_ctx[XHCI_MAX_EPS - 1];
} xhci_input_device_ctx_t;

/*
 * DMA supporting 'context'
 * FIXME: size?
 */
typedef struct xhci_dma_ctx {
    uint32_t type;
    uint32_t size;
    uint8_t* data;
    paddr_t dma_addr;
} xhci_input_dma_t;

#define XHCI_DMA_CTX_DEVICE 0x01
#define XHCI_DMA_CTX_INPUT 0x02

typedef struct xhci_dev_ctx_array {
    uint64_t dev_ctx_ptrs[XHCI_MAX_HC_SLOTS];
    paddr_t dma;
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
#define TRB_LIN 6
#define TRB_EVENT_DATA 7
/* Transfer Ring No-op (not for the command ring) */
#define TRB_TR_NOOP 8
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
#define TRB_SET_DEQ 16
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
#define TRB_PORT_STATUS 34
/* Bandwidth Request Event (opt) */
#define TRB_BANDWIDTH_EVENT 35
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
#define TRB_NEC_CMD_COM 48
/* Get NEC firmware revision. */
#define TRB_NEC_GET_FW 49

/*
 * Xhci ring segment
 *
 * Ring segments allow for non-contiguous rings
 * These types of rings work by having multiple segments of (most likely) 256 trbs per segment
 * whith the last trb being a linkt to the start of the next segment.
 *
 * TODO: implement lmao
 */
typedef struct xhci_segment {
    xhci_trb_t* trbs;
    struct xhci_segment* next;
    paddr_t dma;
    /* Linux has bounce buffers. WTF? */
} xhci_segment_t;

#define XHCI_TRBS_PER_SEGMENT (256)

/*
 * xhci transfer descriptor
 */
typedef struct xhci_td {
    /* All the trbs in a td will be contiguous (for now), so we only need one pointer */
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

#define XHCI_RING_TYPE_MAX (XHCI_RING_TYPE_EVENT)

/*
 * The xhci ring is a structure that can be found at
 * transfer rings and such. We basically keep track of the
 * start, end, enqueue and dequeue pointers
 *
 * Right now any ring consists of only one segment (aka 256 trbs) so
 * we need to implement multi-segmenting the trbs together into one scattered
 * ring. This means:
 * @ring_buffer: simply points to the start of our one segment
 * @ring_dma: our dma (physical, dma mapped) address of the segment
 * @ring_size: almost always XHCI_TRBS_PER_SEGMENT * sizeof(xhci_trb_t) = SMALL_PAGE_SIZE
 * @ring_type: our type
 */
typedef struct xhci_ring {
    /* Both these pointers are virtual kernel addresses to the trbs */
    xhci_trb_t* enqueue;
    xhci_trb_t* dequeue;

    xhci_trb_t* last_trb;

    /* Kernel address to the ring buffer */
    void* ring_buffer;
    paddr_t ring_dma;
    uint32_t ring_size;
    uint32_t ring_type;
    uint32_t ring_cycle;
} xhci_ring_t;

/* ring.c */
extern xhci_ring_t* create_xhci_ring(struct xhci_hcd* hcd, uint32_t trb_count, uint32_t type);
extern void destroy_xhci_ring(xhci_ring_t* ring);

extern int xhci_set_cmd_ring(struct xhci_hcd* hcd);

extern int xhci_cmd_ring_enqueue(struct xhci_hcd* hcd, xhci_trb_t* trb);
extern int xhci_ring_enqueue(struct xhci_hcd* hcd, xhci_ring_t* ring, xhci_trb_t* trb);
extern int xhci_ring_dequeue(struct xhci_hcd* hcd, xhci_ring_t* ring, xhci_trb_t* b_trb);

/*
 * Scratchpad store for a xhci hcd
 *
 * @array: stores the dma addresses of the buffers
 * @dma: stores the dma addresss of the scratchpad array
 * @buffer: store the kernel addresses for the buffers
 */
typedef struct xhci_scratchpad {
    paddr_t* array;
    paddr_t dma;
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
    paddr_t dma;
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

typedef struct xhci_interrupter {
    xhci_ring_t* event_ring;
    xhci_erst_t erst;
    xhci_intr_regs_t* ir_regs;
    uint32_t inter_num;
} xhci_interrupter_t;

/* ring.c */
extern xhci_interrupter_t* create_xhci_interrupter(struct xhci_hcd* xhci);
extern void destory_xhci_interrupter(xhci_interrupter_t* interrupter);

extern int xhci_add_interrupter(struct xhci_hcd* xhci, xhci_interrupter_t* interrupter, uint32_t num);

/*
 * A xhci port
 *
 * This represents a USB device connected to a xhci hub
 */
typedef struct xhci_port {
    /* Base register address */
    void* base_addr;
    /* Pointer to the entry inside the dctx array entry */
    u64* dcba_entry_addr;
    /* Device context structure located at dcba_entry_addr */
    xhci_device_ctx_t* device_context;

    /* The index of this port */
    uint8_t port_num;
    /* The slot we've been given by xhci (index of dcba_entry_addr) */
    uint8_t device_slot;

    /* Base port struct */
    struct usb_port* p_port;

    enum USB_SPEED speed;
    struct xhci_hub* p_hub;
} xhci_port_t;

extern xhci_port_t* create_xhci_port(struct usb_port* port, struct xhci_hub* hub, enum USB_SPEED speed);
extern void destroy_xhci_port(struct xhci_port* port);
extern void init_xhci_port(xhci_port_t* port, void* base_addr, void* dcba_entry_addr, void* device_ctx, uint8_t port_num, uint8_t slot);

static inline bool __is_active_xhci_port(xhci_port_t* port)
{
    return (port->base_addr != nullptr);
}

/*
 * A xhci hub
 *
 * This structure should keep track of the ports that are connected to
 * it
 */
typedef struct xhci_hub {
    xhci_port_t** ports;
    uint32_t port_count;
    uint32_t port_arr_size;

    struct usb_hub* phub;
} xhci_hub_t;

int create_xhci_hub(xhci_hub_t** phub, struct xhci_hcd* xhci, struct usb_device* udev);
void destroy_xhci_hub(xhci_hub_t* hub);

/* xhci_roothub.c */
extern int xhci_process_rhub_xfer(usb_hub_t* hub, usb_xfer_t* xfer);

/*
 * Flags to mark the state that the xhc is in
 */
#define XHC_FLAG_HALTED 0x00000001

typedef struct xhci_hcd {
    struct usb_hcd* parent;

    uint32_t xhc_flags;

    mutex_t* event_lock;

    /* These threads are used to do events and trfs by probing */
    thread_t* event_thread;
    thread_t* trf_finish_thread;

    uint8_t sbrn;
    uint8_t cmd_queue_cycle;
    uint16_t hci_version;
    uint8_t max_slots;
    uint16_t max_interrupters;
    uint8_t max_ports;
    uint8_t isoc_threshold;

    uintptr_t register_ptr;
    size_t register_size;
    uint32_t cap_regs_offset;
    uint32_t oper_regs_offset;
    uint32_t runtime_regs_offset;
    uint32_t doorbell_regs_offset;

    /* FIXME: seperate 3.0 and 2.0 roothubs */
    xhci_hub_t* rhub;

    xhci_cap_regs_t* cap_regs;
    xhci_op_regs_t* op_regs;
    xhci_runtime_regs_t* runtime_regs;
    xhci_db_array_t* db_arr;

    uint32_t scratchpad_count;

    /* Cache for 1024-byte aligned device ctx structures */
    zone_allocator_t* device_ctx_allocator;

    /* We allocate these things ourselves for the xhci controller */
    xhci_dev_ctx_array_t* dctx_array_ptr;
    xhci_ring_t* cmd_ring_ptr;
    xhci_interrupter_t* interrupter;
    xhci_scratchpad_t* scratchpad_ptr;
} xhci_hcd_t;

int xhci_get_port_sts(struct xhci_hcd* xhci, u8 idx, usb_port_status_t* status);
int xhci_clear_port_ftr(struct xhci_hcd* xhci, u8 idx, u32 feature);
int xhci_set_port_ftr(struct xhci_hcd* xhci, u8 idx, u32 feature);

extern void _xhci_event_poll(xhci_hcd_t* xhci);

static inline xhci_hcd_t* hcd_to_xhci(usb_hcd_t* hcd)
{
    return (xhci_hcd_t*)(hcd->private);
}

#define XHCI_CAP_OF(xhci_hub) ((xhci_hub)->cap_regs_offset)
#define XHCI_RR_OF(xhci_hub) ((xhci_hub)->runtime_regs_offset)
#define XHCI_OP_OF(xhci_hub) ((xhci_hub)->oper_regs_offset)

/*
 * These macros are only safe to use after the xhci device has discovered all their offsets
 * we also assume 32-bit I/O so TODO: figure out if there are ever exceptions to this
 */
#define xhci_cap_read(hub, offset) (xhci_read32((hub)->parent, (hub)->cap_regs_offset + (offset)))
#define xhci_cap_write(hub, offset, value) (xhci_write32((hub)->parent, (hub)->cap_regs_offset + (offset), (value)))

#define xhci_oper_read(hub, offset) (xhci_read32((hub)->parent, (hub)->oper_regs_offset + (offset)))
#define xhci_oper_write(hub, offset, value) (xhci_write32((hub)->parent, (hub)->oper_regs_offset + (offset), (value)))

#define xhci_runtime_read(hub, offset) (xhci_read32((hub)->parent, (hub)->runtime_regs_offset + (offset)))
#define xhci_runtime_write(hub, offset, value) (xhci_write32((hub)->parent, (hub)->runtime_regs_offset + (offset), (value)))

#define xhci_doorbell_read(hub, offset) (xhci_read32((hub)->parent, (hub)->doorbell_regs_offset + (offset)))
#define xhci_doorbell_write(hub, offset, value) (xhci_write32((hub)->parent, (hub)->doorbell_regs_offset + (offset), (value)))

#endif // !__ANIVA_XHCI_HUB__
