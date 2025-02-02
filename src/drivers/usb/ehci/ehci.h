#ifndef __ANIVA_USB_EHCI_HCD__
#define __ANIVA_USB_EHCI_HCD__

#include "dev/usb/usb.h"
#include "dev/usb/xfer.h"
#include "drivers/usb/ehci/buffer.h"
#include "drivers/usb/ehci/ehci_spec.h"
#include "mem/zalloc/zalloc.h"
#include "sync/mutex.h"
#include <libk/stddef.h>

struct usb_hcd;
struct usb_device;
struct usb_xfer;

#define EHCI_SPINUP_LIMIT 16

#define EHCI_INT_ENTRY_COUNT 8

#define EHCI_HCD_FLAG_STOPPING 0x00000001

typedef struct ehci_hcd {
    struct usb_hcd* hcd;

    size_t register_size;
    void* capregs;
    void* opregs;

    uint32_t ehci_flags;
    uint32_t hcs_params;
    uint32_t hcc_params;
    uint32_t portcount;
    uint32_t cur_interrupt_state;

    /* Bitfield that tells us which ports have been reset */
    uint32_t port_reset_bits;

    /* Periodic table stuffskis */
    uint32_t periodic_size;
    uint32_t n_itd; // TODO: Replace with something useful (This is always == periodic_size)
    uint32_t* periodic_table;
    paddr_t periodic_dma;
    ehci_qh_t** interrupt_list;
    ehci_itd_t** itd_list;
    ehci_sitd_t** sitd_list;

    /* Scratch buffer for transfer pages */
    ehci_scratch_buffer_t xfer_buf;

    /* DMA Pools for the EHCI datastructures */
    zone_allocator_t* qh_pool;
    zone_allocator_t* qtd_pool;
    zone_allocator_t* itd_pool;
    zone_allocator_t* sitd_pool;

    /* Asynchronous thingy */
    ehci_qh_t* async;

    list_t* transfer_list;
    queue_t* destroyable_qh_q;
    u32 n_destroyable_qh;

    thread_t* interrupt_polling_thread;
    thread_t* transfer_finish_thread;
    thread_t* qhead_cleanup_thread;
    mutex_t* cleanup_lock;
    mutex_t* transfer_lock;
    mutex_t* async_lock;
} ehci_hcd_t;

extern int ehci_process_hub_xfer(usb_hub_t* hub, usb_xfer_t* xfer);

extern int ehci_get_port_sts(ehci_hcd_t* ehci, uint32_t port, usb_port_status_t* status);
extern int ehci_set_port_feature(ehci_hcd_t* ehci, uint32_t port, uint16_t feature);
extern int ehci_clear_port_feature(ehci_hcd_t* ehci, uint32_t port, uint16_t feature);

/* transfer.c */
extern ehci_qh_t* create_ehci_qh(ehci_hcd_t* ehci, struct usb_xfer* xfer);
extern void ehci_init_qh(ehci_qh_t* qh, usb_xfer_t* xfer);
extern error_t destroy_ehci_qh(ehci_hcd_t* ehci, ehci_qh_t* qh);

extern ehci_qtd_t* create_ehci_qtd(ehci_hcd_t* ehci, struct usb_xfer* xfer, ehci_qh_t* qh);

/*
 * Simple wrapper for binding our system-wide usb_xfer struct with an EHCI qh
 * This struct does not own any of the pointers it holds, which means that is
 * for the owner of this struct to manage. Here is a simple overview of the ownership:
 * 1) xfer: Owned by the submitter of the transfer
 * 2) qh: Owned by the EHCI hcd
 */
typedef struct ehci_xfer {
    struct usb_xfer* xfer;
    struct ehci_qh* qh;
    struct ehci_qtd* data_qtd;
} ehci_xfer_t;

extern int ehci_init_ctl_queue(ehci_hcd_t* ehci, struct usb_xfer* xfer, struct ehci_xfer** e_xfer, ehci_qh_t* qh);
extern int ehci_init_data_queue(ehci_hcd_t* ehci, struct usb_xfer* xfer, struct ehci_xfer** e_xfer, ehci_qh_t* qh);

extern ehci_xfer_t* create_ehci_xfer(struct usb_xfer* xfer, ehci_qh_t* qh, ehci_qtd_t* data_qtd);
extern void destroy_ehci_xfer(ehci_hcd_t* ehci, ehci_xfer_t* xfer);

extern int ehci_xfer_finalise(ehci_hcd_t* ehci, ehci_xfer_t* xfer);

extern int ehci_enq_xfer(ehci_hcd_t* ehci, ehci_xfer_t* xfer);
extern int ehci_deq_xfer(ehci_hcd_t* ehci, ehci_xfer_t* xfer);

extern int ehci_init_xfer();
extern int ehci_fini_xfer();

#endif // !__ANIVA_USB_EHCI_HCD__
