/*
 * The pci driver for the xHCI host controller
 */

#include "dev/debug/serial.h"
#include "dev/driver.h"
#include "dev/kterm/kterm.h"
#include "dev/usb/usb.h"
#include "dev/usb/xhci/xhci.h"
#include "libk/flow/error.h"
#include "libk/io.h"
#include "libk/string.h"
#include "mem/heap.h"
#include "mem/kmem_manager.h"
#include "system/asm_specifics.h"
#include <dev/pci/pci.h>
#include <dev/pci/definitions.h>

int xhci_init();
int xhci_exit();
uintptr_t xhci_msg(aniva_driver_t* this, dcc_t code, void* buffer, size_t size, void* out_buffer, size_t out_size);

pci_dev_id_t xhci_pci_ids[] = {
  PCI_DEVID_CLASSES(SERIAL_BUS_CONTROLLER, PCI_SUBCLASS_SBC_USB, PCI_PROGIF_XHCI),
  PCI_DEVID_END,
};

static xhci_hub_t* to_xhci_safe(usb_hcd_t* hub)
{
  xhci_hub_t* ret = hub->private;
  ASSERT_MSG(ret && hub->hub_type == USB_HUB_TYPE_XHCI && ret->register_ptr, "No (valid) XHCI hub attached to this hub!");

  return ret;
}

static uint64_t xhci_read64(usb_hcd_t* hub, uintptr_t offset)
{
  return *(volatile uint64_t*)(to_xhci_safe(hub)->register_ptr + offset);
}

static void xhci_write64(usb_hcd_t* hub, uintptr_t offset, uint64_t value)
{
  *(volatile uint64_t*)(to_xhci_safe(hub)->register_ptr + offset) = value;
}

static uint32_t xhci_read32(usb_hcd_t* hub, uintptr_t offset)
{
  return *(volatile uint32_t*)(to_xhci_safe(hub)->register_ptr + offset);
}

static void xhci_write32(usb_hcd_t* hub, uintptr_t offset, uint32_t value)
{
  *(volatile uint32_t*)(to_xhci_safe(hub)->register_ptr + offset) = value;
}

static uint16_t xhci_read16(usb_hcd_t* hub, uintptr_t offset)
{
  return *(volatile uint16_t*)(to_xhci_safe(hub)->register_ptr + offset);
}

static void xhci_write16(usb_hcd_t* hub, uintptr_t offset, uint16_t value)
{
  *(volatile uint16_t*)(to_xhci_safe(hub)->register_ptr + offset) = value;
}

static uint8_t xhci_read8(usb_hcd_t* hub, uintptr_t offset)
{
  return *(volatile uint8_t*)(to_xhci_safe(hub)->register_ptr + offset);
}

static void xhci_write8(usb_hcd_t* hub, uintptr_t offset, uint8_t value)
{
  *(volatile uint8_t*)(to_xhci_safe(hub)->register_ptr + offset) = value;
}

static int xhci_wait_read(struct usb_hcd* hub, uintptr_t max_timeout, uintptr_t offset, uintptr_t mask, uintptr_t expect)
{
  kernel_panic("TODO implement wait read");
  return 0;
}

usb_hcd_mmio_ops_t xhci_mmio_ops = {
  .mmio_read64 = xhci_read64,
  .mmio_write64 = xhci_write64,
  .mmio_write32 = xhci_write32,
  .mmio_read32 = xhci_read32,
  .mmio_read16 = xhci_read16,
  .mmio_write16 = xhci_write16,
  .mmio_read8 = xhci_read8,
  .mmio_write8 = xhci_write8,

  .mmio_wait_read = xhci_wait_read,
};

static xhci_hub_t* create_xhci_hub()
{
  xhci_hub_t* hub;

  hub = kmalloc(sizeof(xhci_hub_t));

  if (!hub)
    return nullptr;

  memset(hub, 0, sizeof(*hub));

  return hub;
}

int xhci_probe(pci_device_t* device, pci_driver_t* driver)
{
  int error;
  usb_hcd_t* hub;
  xhci_hub_t* xhci_hub;
  paddr_t xhci_register_p;
  size_t xhci_register_size;
  uint32_t bar0 = 0, bar1 = 0;

  println("Probing for XHCI");

  hub = create_usb_hcd(device, nullptr, USB_HUB_TYPE_XHCI);
  xhci_hub = create_xhci_hub();

  error = register_usb_hcd(hub);

  /* FUCKK */
  if (error)
    return error;

  hub->private = xhci_hub;
  hub->mmio_ops = &xhci_mmio_ops;

  /* Enable the HC */
  pci_device_enable(device);

  device->ops.read_dword(device, BAR0, &bar0);
  device->ops.read_dword(device, BAR1, &bar1);

  xhci_register_size = pci_get_bar_size(device, 0);
  xhci_register_p = get_bar_address(bar0);

  if (is_bar_64bit(bar0))
    xhci_register_p |= get_bar_address(bar1) << 32;

  xhci_hub->register_ptr = Must(__kmem_kernel_alloc(
      xhci_register_p, xhci_register_size, NULL, KMEM_FLAG_KERNEL | KMEM_FLAG_DMA));

  xhci_hub->cap_regs_offset = 0;
  xhci_hub->cap_regs = (xhci_cap_regs_t*)xhci_hub->register_ptr;

  xhci_hub->oper_regs_offset = HC_LENGTH(xhci_read32(hub, 0));
  xhci_hub->op_regs = (xhci_op_regs_t*)(xhci_hub->register_ptr + XHCI_OP_OF(xhci_hub));

  xhci_hub->runtime_regs_offset = xhci_read32(hub, GET_OFFSET(xhci_cap_regs_t, runtime_regs_offset));
  xhci_hub->runtime_regs = (xhci_runtime_regs_t*)(xhci_hub->register_ptr + XHCI_RR_OF(xhci_hub));

  uint32_t hc_version = HC_VERSION(mmio_read_dword(&xhci_hub->cap_regs->hc_capbase));
  uint32_t cap_len = HC_LENGTH(xhci_read32(hub, 0));

  println_kterm("XHCI cap len: ");
  println_kterm(to_string(cap_len));
  println_kterm("XHCI version: ");
  println_kterm(to_string(hc_version));
  kernel_panic(to_string(xhci_register_size));

  return 0;
}

int xhci_remove(pci_device_t* device)
{

  return 0;
}

/*
 * xhci is hosted through the PCI bus, so we'll register it through that
 */
pci_driver_t xhci_pci_driver = {
  .f_probe = xhci_probe,
  .f_pci_on_remove = xhci_remove,
  .id_table = xhci_pci_ids,
  .device_flags = NULL,
};

aniva_driver_t xhci_driver = {
  .m_name = "xhci",
  .f_init = xhci_init,
  .f_exit = xhci_exit,
  .f_msg = xhci_msg,
  .m_version = DEF_DRV_VERSION(0, 0, 1),
};
EXPORT_DRIVER_PTR(xhci_driver);

uintptr_t xhci_msg(aniva_driver_t* this, dcc_t code, void* buffer, size_t size, void* out_buffer, size_t out_size)
{
  return 0;
}

int xhci_init()
{
  register_pci_driver(&xhci_pci_driver);

  return 0;
}

int xhci_exit()
{
  unregister_pci_driver(&xhci_pci_driver);

  return 0;
}
