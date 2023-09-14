#ifndef __ANIVA_USB_REQUEST__
#define __ANIVA_USB_REQUEST__

#include "libk/flow/doorbell.h"
#include "libk/flow/reference.h"
#include <libk/stddef.h>

struct usb_device;

/*
 * Generic USB request structure for the 
 * entire subsystem
 */
typedef struct usb_request {
  flat_refc_t ref;

  void* req_buffer;
  paddr_t req_dma_addr;
  uint32_t req_size;
  uint32_t req_tranfered_size;

  struct usb_device* device;

  /* Doorbell for async status reports */
  kdoorbell_t* req_doorbell;
} usb_request_t;

usb_request_t* create_usb_req(void* buffer, uint32_t buffer_size);

/* Manage the existance of the request object with a reference counter */
void get_usb_req(usb_request_t* req);
void release_usb_req(usb_request_t* req);

void usb_post_request(usb_request_t* req);
void usb_cancel_request(usb_request_t* req);
bool usb_await_req_complete(usb_request_t* req, uint32_t max_timeout);

#endif // !__ANIVA_USB_REQUEST__
