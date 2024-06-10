#ifndef __ANIVA_USB_XFER__
#define __ANIVA_USB_XFER__

/*
 * High level USB request
 *
 * Requests simply get posted to the USB subsystem, which passes it to the correct devices
 * based on the information inside the request object
 */

#include "libk/flow/doorbell.h"
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

#define USB_XFER_FLAG_DONE 0x0001
#define USB_XFER_FLAG_CANCELED 0x0002
#define USB_XFER_FLAG_ERROR 0x0004
#define USB_XFER_FLAG_DATA_TGL 0x0008

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

    paddr_t resp_dma_addr;
    paddr_t req_dma_addr;

    uint32_t resp_size;
    uint32_t req_size;

    uint32_t req_tranfered_size;
    /* What interface do we want to communicate with */
    uint32_t req_endpoint;
    uint32_t req_devaddr;
    uint32_t req_hubaddr;
    uint32_t req_hubport;
    uint16_t req_max_packet_size;

    uint16_t xfer_flags;

    struct usb_device* device;

    /* Doorbell for async status reports */
    kdoor_t* req_door;
} usb_xfer_t;

static inline bool usb_xfer_is_done(usb_xfer_t* xfer)
{
    return ((xfer->xfer_flags & USB_XFER_FLAG_DONE) == USB_XFER_FLAG_DONE);
}

usb_xfer_t* create_usb_xfer(struct usb_device* device, kdoorbell_t* completion_db, enum USB_XFER_TYPE type, enum USB_XFER_DIRECTION direction, uint8_t devaddr, uint8_t hubaddr, uint8_t hubport, uint32_t endpoint, void* req_buf, uint32_t req_bsize, void* resp_buf, uint32_t resp_bsize);

int init_ctl_xfer(usb_xfer_t** pxfer, kdoorbell_t** pdb, struct usb_ctlreq* ctl, struct usb_device* target, uint8_t devaddr, uint8_t hubaddr, uint8_t hubport, uint8_t reqtype, uint8_t req, uint16_t value, uint16_t idx, uint16_t len, void* respbuf, uint32_t respbuf_len);

/* Manage the existance of the request object with a reference counter */
void get_usb_xfer(usb_xfer_t* req);
void release_usb_xfer(usb_xfer_t* req);

int usb_xfer_complete(usb_xfer_t* xfer);
int usb_xfer_enqueue(usb_xfer_t* xfer, struct usb_hcd* hcd);
int usb_xfer_get_max_packet_size(usb_xfer_t* xfer, size_t* bsize);

void usb_post_xfer(usb_xfer_t* req, uint8_t type);
void usb_cancel_xfer(usb_xfer_t* req);
bool usb_await_xfer_complete(usb_xfer_t* req, uint32_t max_timeout);

#endif // !__ANIVA_usb_xfer__
