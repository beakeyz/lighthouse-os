
#include "libk/flow/error.h"
#include "sched/scheduler.h"
#include "xhci.h"

/*!
 * @brief: Main entrypoint for our event probe thread
 */
void _xhci_event_poll(xhci_hcd_t* xhci)
{
    uint32_t type;
    xhci_ring_t* event_ring;

    event_ring = xhci->interrupter->event_ring;

    event_ring->ring_cycle = 1;

    /* Loop forever and check the event status bits on the hcd */
    do {
        if ((event_ring->dequeue->control & 1) != event_ring->ring_cycle) {
            scheduler_yield();
            continue;
        }

        type = XHCI_TRB_FIELD_TO_TYPE(event_ring->dequeue->control);

        printf("TYpe=%d\n", type);
        kernel_panic("Got an event!");
    } while (true);
}
