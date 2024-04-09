#include "dev/usb/spec.h"
#include "dev/usb/xfer.h"
#include "ehci.h"
#include <dev/usb/hcd.h>

int ehci_process_hub_xfer(usb_hub_t* hub, usb_xfer_t* xfer)
{
  int error;
  ehci_hcd_t* ehci;
  usb_ctlreq_t* usbctl;

  /* Hub can only process ctl transfers */
  if (xfer->req_type != USB_CTL_XFER)
    return -KERR_INVAL;

  ehci = hub->hcd->private;

  if (!ehci)
    return -KERR_INVAL;

  usbctl = xfer->req_buffer;

  if (!usbctl)
    return -KERR_INVAL;

  /* Fail by default */
  error = -KERR_INVAL;

  printf("Got EHCI Roothub transfer of type %d\n", usbctl->request);

  switch (usbctl->request) {
    case USB_REQ_GET_STATUS:
      /* We're going to need a response destination ='[ */
      if (!xfer->resp_buffer || xfer->resp_size != sizeof(usb_port_status_t))
        break;

      xfer->req_tranfered_size = sizeof(usb_port_status_t);

      /* Report our own status (all zero = OK) */
      memset(xfer->resp_buffer, 0, xfer->resp_size);

      error = KERR_NONE;

      if (usbctl->index)
        error = ehci_get_port_sts(ehci, usbctl->index-1, xfer->resp_buffer);

      break;
    case USB_REQ_CLEAR_FEATURE:
      if (!usbctl->index)
        break;

      error = ehci_clear_port_feature(ehci, usbctl->index-1, usbctl->value);
      break;
    default:
      break;
  }

  usb_xfer_complete(xfer);
  return error;
}
