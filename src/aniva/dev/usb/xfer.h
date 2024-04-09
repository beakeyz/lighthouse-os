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

struct usb_hub;
struct usb_device;

enum USB_XFER_TYPE {
  USB_CTL_XFER,
  USB_INT_XFER,
  USB_BULK_XFER,
  USB_ISO_XFER,
};

/*
 * Generic USB request structure for the 
 * entire subsystem
 *
 * HCD drivers should construct the correct commands based on the request types
 * sent here.
 */
typedef struct usb_xfer {
  flat_refc_t ref;
  enum USB_XFER_TYPE req_type;

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

  struct usb_device* device;

  /* Doorbell for async status reports */
  kdoor_t* req_door;
} usb_xfer_t;

usb_xfer_t* create_usb_xfer(struct usb_device* device, kdoorbell_t* completion_db, void* buffer, uint32_t buffer_size);

/* Manage the existance of the request object with a reference counter */
void get_usb_xfer(usb_xfer_t* req);
void release_usb_xfer(usb_xfer_t* req);

int usb_xfer_complete(usb_xfer_t* xfer);

int usb_xfer_enqueue(usb_xfer_t* xfer, struct usb_hub* hub);

void usb_post_xfer(usb_xfer_t* req, uint8_t type);
void usb_cancel_xfer(usb_xfer_t* req);
bool usb_await_xfer_complete(usb_xfer_t* req, uint32_t max_timeout);

#endif // !__ANIVA_usb_xfer__
