#ifndef __ANIVA_USB_XFER__
#define __ANIVA_USB_XFER__

/*
 * High level USB request
 *
 * Requests simply get posted to the USB subsystem, which passes it to the correct devices
 * based on the information inside the request object
 */

#include "libk/flow/reference.h"
#include <libk/stddef.h>

struct usb_hcd;
struct usb_device;
struct usb_ctlreq;

enum USB_XFER_TYPE {
    USB_CTL_XFER,
    USB_INT_XFER,
    USB_BULK_XFER,
    USB_ISO_XFER,
};

enum USB_XFER_DIRECTION {
    USB_DIRECTION_HOST_TO_DEVICE,
    USB_DIRECTION_DEVICE_TO_HOST,
};

/* This transfer has been handled by the targeted USB device */
#define USB_XFER_FLAG_DONE 0x01
/* Transfer has been canceled by the transferer */
#define USB_XFER_FLAG_CANCELED 0x02
/* An error orrured during this transfer */
#define USB_XFER_FLAG_ERROR 0x04
/* ??? */
#define USB_XFER_FLAG_DATA_TGL 0x08
/* This transfer has been completed and needs a requeue */
#define USB_XFER_FLAG_REQUEUE 0x10
#define USB_XFER_FLAG_COMPLETED 0x20

/*!
 * Generic USB request structure for the
 * entire subsystem
 *
 * HCD drivers should construct the correct commands based on the request types
 * sent here.
 *
 * @req_buffer: The buffer that contains information relevant to the request
 * @resp_buffer: The buffer that will be sent next to the request buffer. Most of the
 * time this will be used to put the response of the transfer in, but it could also be
 * used as a generic data transfer buffer (In the case of a bulk transfer to the USB
 * device)
 */
typedef struct usb_xfer {
    flat_refc_t ref;
    enum USB_XFER_TYPE req_type;
    enum USB_XFER_DIRECTION req_direction;

    void* resp_buffer;
    void* req_buffer;

    /* Private xfer-specific context */
    void* priv_ctx;

    paddr_t resp_dma_addr;
    paddr_t req_dma_addr;

    uint32_t resp_size;
    uint32_t req_size;

    uint32_t req_tranfered_size;
    /* What interface do we want to communicate with */
    uint32_t req_endpoint;
    uint32_t req_devaddr;
    uint16_t req_hubaddr;
    uint16_t req_hubport;
    uint16_t req_max_packet_size;
    uint8_t xfer_flags;
    uint8_t xfer_interval;
    uint32_t resp_transfer_size;

    struct usb_device* device;

    /* Called when there is no door(bell) registered */
    int (*f_completion_cb)(struct usb_xfer*);

    /* Doorbell for status reports */
    struct semaphore* status_sem;
} usb_xfer_t;

static inline bool usb_xfer_is_done(usb_xfer_t* xfer)
{
    return ((xfer->xfer_flags & USB_XFER_FLAG_DONE) == USB_XFER_FLAG_DONE);
}

static inline bool usb_xfer_is_err(usb_xfer_t* xfer)
{
    return ((xfer->xfer_flags & USB_XFER_FLAG_ERROR) == USB_XFER_FLAG_ERROR);
}

usb_xfer_t* create_usb_xfer(struct usb_device* device, int (*f_cb)(struct usb_xfer*), enum USB_XFER_TYPE type, enum USB_XFER_DIRECTION direction, uint8_t devaddr, uint8_t hubaddr, uint8_t hubport, uint8_t endpoint, uint16_t max_pckt_size, uint8_t interval, void* req_buf, uint32_t req_bsize, void* resp_buf, uint32_t resp_bsize);

int init_ctl_xfer(usb_xfer_t** pxfer, struct usb_ctlreq* ctl, struct usb_device* target, uint8_t devaddr, uint8_t hubaddr, uint8_t hubport, uint8_t reqtype, uint8_t req, uint16_t value, uint16_t idx, uint16_t len, void* respbuf, uint32_t respbuf_len);
int init_int_xfer(usb_xfer_t** pxfer, int (*f_cb)(struct usb_xfer*), struct usb_device* target, enum USB_XFER_DIRECTION direction, uint8_t endpoint, uint16_t max_pckt_size, uint8_t interval, void* buffer, size_t bsize);

/* Manage the existance of the request object with a reference counter */
void get_usb_xfer(usb_xfer_t* req);
void release_usb_xfer(usb_xfer_t* req);

int usb_xfer_complete(usb_xfer_t* xfer);
int usb_xfer_enqueue(usb_xfer_t* xfer, struct usb_hcd* hcd);
int usb_xfer_requeue(usb_xfer_t* xfer, struct usb_hcd* hcd);
int usb_xfer_cancel(usb_xfer_t* xfer);
int usb_xfer_get_max_packet_size(usb_xfer_t* xfer, size_t* bsize);

void usb_post_xfer(usb_xfer_t* req, uint8_t type);
void usb_cancel_xfer(usb_xfer_t* req);
bool usb_xfer_await_complete(usb_xfer_t* req, uint32_t max_timeout);

#endif // !__ANIVA_usb_xfer__
