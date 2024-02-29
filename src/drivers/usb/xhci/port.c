#include "mem/heap.h"
#include "xhci.h"

xhci_port_t* create_xhci_port(struct xhci_hub* hub, enum USB_SPEED speed)
{
  xhci_port_t* port;

  if (!hub)
    return nullptr;

  port = kmalloc(sizeof(*port));

  if (!port)
    return nullptr;

  memset(port, 0, sizeof(*port));

  port->p_hub = hub;
  port->speed = speed;

  return port;
}

void init_xhci_port(xhci_port_t* port, void* base_addr, void* dcba_entry_addr, void* device_ctx, uint8_t port_num, uint8_t slot)
{
  port->base_addr = base_addr;
  port->dcba_entry_addr = dcba_entry_addr;
  port->device_context = device_ctx;
  port->port_num = port_num;
  port->device_slot = slot;
}

void destroy_xhci_port(struct xhci_port* port)
{
  kfree(port);
}
