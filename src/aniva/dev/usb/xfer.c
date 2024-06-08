#include "xfer.h"
#include "dev/usb/spec.h"
#include "dev/usb/usb.h"
#include "dev/usb/hcd.h"
#include "libk/flow/doorbell.h"
#include "libk/flow/error.h"
#include "libk/flow/reference.h"
#include "mem/heap.h"
#include "sync/mutex.h"
#include <sched/scheduler.h>

/*!
 * @brief Create an empty usb request attached to a device
 *
 * Nothing to add here...
 */
usb_xfer_t* create_usb_xfer(
    struct usb_device* device,
    kdoorbell_t* completion_db,
    enum USB_XFER_TYPE type,
    enum USB_XFER_DIRECTION direction,
    uint8_t devaddr,
    uint8_t hubaddr,
    uint8_t hubport,
    uint32_t endpoint,
    void* req_buf,
    uint32_t req_bsize,
    void* resp_buf,
    uint32_t resp_bsize
)
{
  usb_xfer_t* request;

  /* Zeroes the thing */
  request = allocate_usb_xfer();

  if (!request)
    return nullptr;

  request->ref = 1;
  request->device = device;
  request->req_direction = direction;
  request->req_type = type;

  request->req_devaddr = devaddr;
  request->req_hubaddr = hubaddr;
  request->req_hubport = hubport;
  request->req_endpoint = endpoint;

  request->req_buffer = req_buf;
  request->req_size = req_bsize;
  request->resp_buffer = resp_buf;
  request->resp_size = resp_bsize;
  request->req_door = kmalloc(sizeof(kdoor_t));

  /* Initialize the door */
  init_kdoor(request->req_door, NULL, NULL);

  /* Make sure the door is registered */
  register_kdoor(completion_db, request->req_door);

  return request;
}

static void destroy_usb_req(usb_xfer_t* req)
{
  if (req->req_door && req->req_door->m_bell)
    unregister_kdoor(req->req_door->m_bell, req->req_door);

  kfree(req->req_door);
  deallocate_usb_xfer(req);
}

/*!
 * @brief: Allocate an prepare a control transfer to @devaddr and @hubport
 */
int init_ctl_xfer(usb_xfer_t** pxfer, kdoorbell_t** pdb, usb_ctlreq_t* ctl, usb_device_t* target, uint8_t devaddr, uint8_t hubaddr, uint8_t hubport, uint8_t reqtype, uint8_t req, uint16_t value, uint16_t idx, uint16_t len, void* respbuf, uint32_t respbuf_len)
{
  enum USB_XFER_DIRECTION dir;
  kdoorbell_t* db;
  usb_xfer_t* xfer;

  dir = USB_DIRECTION_HOST_TO_DEVICE;

  /* Detect the transfer direction */
  if ((reqtype & USB_TYPE_DEV_IN) == USB_TYPE_DEV_IN)
    dir = USB_DIRECTION_DEVICE_TO_HOST;

  *ctl = (usb_ctlreq_t) {
    .request_type = reqtype,
    .request = req,
    .value = value,
    .index = idx,
    .length = len,
  };

  db = create_doorbell(1, NULL);
  xfer = create_usb_xfer(target, db, USB_CTL_XFER, dir, devaddr, hubaddr, hubport, 0, ctl, sizeof(*ctl), respbuf, respbuf_len);

  xfer->req_max_packet_size = 8;

  if (target->desc.type)
    xfer->req_max_packet_size = target->desc.max_pckt_size;

  *pxfer = xfer;
  *pdb = db;
  return 0;
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

int usb_xfer_complete(usb_xfer_t* xfer)
{
  doorbell_ring(xfer->req_door->m_bell);
  return 0;
}

/*!
 * @brief: Directly submit a transfer to a hub
 *
 * This assumes the fields in @xfer are setup correctly
 */
int usb_xfer_enqueue(usb_xfer_t* xfer, struct usb_hcd* hcd)
{
  int error; 

  if (!hcd)
    return -KERR_INVAL;

  if (!hcd->io_ops || !hcd->io_ops->enq_request)
    return -KERR_INVAL;

  mutex_lock(hcd->hcd_lock);

  error = hcd->io_ops->enq_request(hcd, xfer);

  mutex_unlock(hcd->hcd_lock);

  return error;
}

static inline int _usb_get_ctl_packet_size(usb_device_t* udev, size_t* bsize)
{
  switch (udev->speed) {
    case USB_LOWSPEED:
      *bsize = 8;
      break;
    case USB_HIGHSPEED:
      *bsize = 64;
      break;
    case USB_SUPERSPEED:
      *bsize = 512;
      break;
    default:
      break;
  }
  return 0;
}

static inline int _usb_get_bulk_packet_size(usb_device_t* udev, size_t* bsize)
{
  switch (udev->speed) {
    case USB_HIGHSPEED:
      *bsize = 512;
      break;
    case USB_FULLSPEED:
      *bsize = 1024;
      break;
    default:
      break;
  }
  return 0;
}

int usb_xfer_get_max_packet_size(usb_xfer_t* xfer, size_t* bsize)
{
  size_t size = xfer->req_max_packet_size;

  if (!xfer || !xfer->device || !bsize)
    return -1;

  switch (xfer->req_type) {
    case USB_CTL_XFER:
      _usb_get_ctl_packet_size(xfer->device, &size);
      break;
    case USB_BULK_XFER:
      _usb_get_bulk_packet_size(xfer->device, &size);
      break;
    default:
      break;
  }

  *bsize = size;
  return 0;
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

