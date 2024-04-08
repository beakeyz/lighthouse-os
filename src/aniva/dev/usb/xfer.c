#include "xfer.h"
#include "dev/usb/usb.h"
#include "libk/flow/doorbell.h"
#include "libk/flow/error.h"
#include "libk/flow/reference.h"
#include "mem/heap.h"
#include <sched/scheduler.h>

/*!
 * @brief Create an empty usb request attached to a device
 *
 * Nothing to add here...
 */
usb_xfer_t* create_usb_xfer(struct usb_device* device, void* buffer, uint32_t buffer_size)
{
  usb_xfer_t* request;

  request = allocate_usb_xfer();

  if (!request)
    return nullptr;

  request->ref = 1;
  request->device = device;
  request->req_buffer = buffer;
  request->req_size = buffer_size;
  request->req_door = kmalloc(sizeof(kdoor_t));

  init_kdoor(request->req_door, NULL, NULL);

  return request;
}

static void destroy_usb_req(usb_xfer_t* req)
{
  kfree(req->req_door);
  deallocate_usb_xfer(req);
}

/* 
 * Manage the existance of the request object with a reference counter 
 */
void get_usb_xfer(usb_xfer_t* req)
{
  flat_ref(&req->ref);
}

void release_usb_xfer(usb_xfer_t* req)
{
  flat_unref(&req->ref);

  if (!req->ref)
    destroy_usb_req(req);
}

/*!
 * @brief Post a USB request to the device
 *
 * Nothing to add here...
 */
void usb_post_xfer(usb_xfer_t* req, uint8_t type)
{
  kernel_panic("TODO: usb_post_request");
}

/*!
 * @brief Cancel a USB request
 *
 * Nothing to add here...
 */
void usb_cancel_xfer(usb_xfer_t* req)
{
  kernel_panic("TODO: implement usb_cancel_request");
}

/*!
 * @brief Wait for a USB request to complete
 *
 * Nothing to add here...
 */
bool usb_await_xfer_complete(usb_xfer_t* req, uint32_t max_timeout)
{
  while (!kdoor_is_rang(req->req_door)) {

    if (max_timeout) {
      max_timeout--;

      if (!max_timeout)
        break;
    }

    scheduler_yield();
  }

  return (kdoor_is_rang(req->req_door));
}

