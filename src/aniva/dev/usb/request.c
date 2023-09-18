#include "request.h"
#include "dev/usb/hcd.h"
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
usb_request_t* create_usb_req(struct usb_device* device, void* buffer, uint32_t buffer_size)
{
  usb_request_t* request;

  request = allocate_usb_request();

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

static void destroy_usb_req(usb_request_t* req)
{
  kfree(req->req_door);
  deallocate_usb_request(req);
}

/* 
 * Manage the existance of the request object with a reference counter 
 */
void get_usb_req(usb_request_t* req)
{
  flat_ref(&req->ref);
}

void release_usb_req(usb_request_t* req)
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
void usb_post_request(usb_request_t* req, uint8_t type)
{
  usb_hub_t* hub;
  usb_hcd_t* hcd;

  if (!req->device || !usb_req_is_type_valid(type))
    return;

  hub = req->device->hub;

  if (!hub)
    return;

  hcd = hub->hcd;

  if (!hcd)
    return;

  req->req_type = type;

  /* Register the door to be able to await request completion */
  Must(register_kdoor(req->device->req_doorbell, req->req_door));

  hcd->io_ops->enq_request(hcd, req);
}

/*!
 * @brief Cancel a USB request
 *
 * Nothing to add here...
 */
void usb_cancel_request(usb_request_t* req)
{
  kernel_panic("TODO: implement usb_cancel_request");
}

/*!
 * @brief Wait for a USB request to complete
 *
 * Nothing to add here...
 */
bool usb_await_req_complete(usb_request_t* req, uint32_t max_timeout)
{
  while (max_timeout && !kdoor_is_rang(req->req_door)) {

    scheduler_yield();
    max_timeout--;
  }

  return (kdoor_is_rang(req->req_door));
}

