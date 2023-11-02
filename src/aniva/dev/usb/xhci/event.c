
#include "dev/usb/xhci/xhci.h"
#include "libk/flow/error.h"

/*!
 * @brief: Main entrypoint for our event probe thread
 */
void probe_events(xhci_hcd_t* xhci)
{
  kernel_panic("Reached probe_events");

  /* Loop forever and check the event status bits on the hcd */
}
