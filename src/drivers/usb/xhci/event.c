
#include "libk/flow/error.h"
#include "sched/scheduler.h"
#include "xhci.h"

/*!
 * @brief: Main entrypoint for our event probe thread
 */
void _xhci_event_poll(xhci_hcd_t* xhci)
{
  xhci_trb_t event_trb;
  xhci_ring_t* evnt_ring;

  evnt_ring = xhci->interrupter->event_ring;

  xhci->interrupter->erst.dma

  /* Loop forever and check the event status bits on the hcd */
  do {
  
    if (xhci_ring_dequeue(xhci, evnt_ring, &event_trb))
      scheduler_yield();

    kernel_panic("Got an event!");
  } while (true);
}
