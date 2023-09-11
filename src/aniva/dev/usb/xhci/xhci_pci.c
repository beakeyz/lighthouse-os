/*
 * The pci driver for the xHCI host controller
 */

#include "dev/debug/serial.h"
#include "dev/driver.h"
#include "dev/kterm/kterm.h"
#include "dev/usb/hcd.h"
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

static xhci_hcd_t* to_xhci_safe(usb_hcd_t* hub)
{
  xhci_hcd_t* ret = hub->private;
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

/*!
 * @brief Read a certain bit (sequence) in a field until we time out or read the correct value
 *
 * @returns: 0 on success, -1 on failure
 */
static int xhci_wait_read(struct usb_hcd* hub, uintptr_t max_timeout, void* address, uintptr_t mask, uintptr_t expect)
{
  uint32_t value;

  while (max_timeout) {
    value = mmio_read_dword(address);

    if ((value & mask) == expect)
      return 0;

    delay(1);
    max_timeout--;
  }

  return -1;
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

/*!
 * @brief Create and populate scratchpad buffers
 *
 * Allocates the memory needed for scratchpads and puts the dma address in the first scratchpad thing in the device context array
 * requires the dca to be allocated
 */
static int xhci_create_scratchpad(xhci_hcd_t* xhci)
{
  xhci_scratchpad_t* sp;

  /* Already set =/ */
  if (xhci->scratchpad_ptr)
    return -1;

  /* No scratchpad =( */
  if (!xhci->scratchpad_count)
    return 0;

  sp = kmalloc(sizeof(*xhci->scratchpad_ptr));

  if (!sp)
    return -1;

  memset(sp, 0, sizeof(xhci_scratchpad_t));

  /* Allocate the dma arrray */
  sp->array = (void*)Must(__kmem_kernel_alloc_range(
      sizeof(uintptr_t) * xhci->scratchpad_count, NULL, KMEM_FLAG_KERNEL | KMEM_FLAG_DMA));

  /* Set the dma (aka physical) address */
  sp->dma = kmem_to_phys(nullptr, (uintptr_t)sp->array);

  /* Allocate the buffers (aka kernel addresses to the scratchpad buffers) */
  sp->buffers = kmalloc(sizeof(void*) * xhci->scratchpad_count);

  /* Allocate all the buffers */
  for (uintptr_t i = 0; i < xhci->scratchpad_count; i++) {
    vaddr_t buffer_addr = Must(__kmem_kernel_alloc_range(SMALL_PAGE_SIZE, NULL, KMEM_FLAG_KERNEL | KMEM_FLAG_DMA));
    paddr_t dma_addr = kmem_to_phys(nullptr, buffer_addr);

    sp->array[i] = dma_addr;
    sp->buffers[i] = (void*)buffer_addr;
  }

  xhci->scratchpad_ptr = sp;

  xhci->dctx_array_ptr->dev_ctx_ptrs[0] = sp->dma;
  return 0;
}

static void xhci_destroy_scratchpad(xhci_hcd_t* xhci)
{
  xhci_scratchpad_t* scratchpad;

  if (!xhci->scratchpad_ptr)
    return;

  scratchpad = xhci->scratchpad_ptr;

  /* Deallocate the buffers */
  for (uintptr_t i = 0; i < xhci->scratchpad_count; i++) {
    __kmem_kernel_dealloc((vaddr_t)scratchpad->buffers[i], SMALL_PAGE_SIZE);
  }

  /* Deallocate the dma array */
  __kmem_kernel_dealloc((vaddr_t)scratchpad->array, sizeof(uintptr_t) * xhci->scratchpad_count);

  /* Free the buffers buffer */
  kfree(scratchpad->buffers);

  /* Lastly, free the scratchpad struct */
  kfree(xhci->scratchpad_ptr);

  /* Make sure the xhci structure knows this is gone */
  xhci->scratchpad_ptr = nullptr;

  /* Also make sure xhci knows its gone */
  if (xhci->dctx_array_ptr) {
    xhci->dctx_array_ptr->dev_ctx_ptrs[0] = NULL;
  }
}

/*!
 * @brief Allocate (DMA) buffers that we and the xhci controller needs
 * 
 * Things we need to do:
 * - Program the max slots
 * - Find out the page size (prob just 4Kib)
 * - Setup port array
 *
 * Things we need to allocate:
 * - Device context array
 * - Scratchpads
 * - Command rings
 */
static int xhci_prepare_memory(usb_hcd_t* hcd)
{
  int error;
  uint32_t hcc_params_1;
  uint32_t hcc_params_2;
  xhci_hcd_t* xhci = hcd->private;

  hcc_params_1 = mmio_read_dword(&xhci->cap_regs->hcc_params_1);
  hcc_params_2 = mmio_read_dword(&xhci->cap_regs->hcc_params_2);

  xhci->max_ports = HC_MAX_PORTS(hcc_params_1);
  xhci->max_slots = HC_MAX_SLOTS(hcc_params_1);

  if (!xhci->max_ports)
    return -1;

  /* Program the max slots */
  mmio_write_dword(&xhci->op_regs->config_reg, xhci->max_slots);

  xhci->dctx_array_ptr = (void*)Must(__kmem_kernel_alloc_range(sizeof(xhci_dev_ctx_array_t), NULL, KMEM_FLAG_KERNEL | KMEM_FLAG_DMA));
  xhci->dctx_array_ptr->dma = kmem_to_phys(nullptr, (vaddr_t)xhci->dctx_array_ptr);

  memset(xhci->dctx_array_ptr, 0, sizeof(xhci_dev_ctx_array_t));

  mmio_write_qword(&xhci->op_regs->dcbaa_ptr, xhci->dctx_array_ptr->dma);

  xhci->scratchpad_count = HC_MAX_SCRTCHPD(hcc_params_2);

  /* NOTE: it is legal for there to be no scratchpad */
  error = xhci_create_scratchpad(xhci);

  if (error)
    return -1;

  /* Set the command ring */
  xhci->cmd_ring_ptr = create_xhci_ring(xhci, XHCI_MAX_COMMANDS, XHCI_RING_TYPE_CMD);

  error = xhci_set_cmd_ring(xhci);

  if (error)
    return -2;

  xhci->interrupter = create_xhci_interrupter(xhci);

  if (!xhci->interrupter)
    return -3;

  error = xhci_add_interrupter(xhci, xhci->interrupter, 0);

  if (error)
    return error;

  return 0;
}

static int xhci_reset(usb_hcd_t* hcd)
{
  int error;
  uint32_t status;
  uint32_t cmd;
  xhci_hcd_t* xhci= hcd->private;

  status = mmio_read_dword(&xhci->op_regs->status);

  if (status == ~(uint32_t)0)
    return -1;

  if ((status & XHCI_STS_HLT) == 0)
    return -2;

  /* Construct the reset command */
  cmd = mmio_read_dword(&xhci->op_regs->cmd) | XHCI_CMD_RESET;

  /* Set the command */
  mmio_write_dword(&xhci->op_regs->cmd, cmd);

  /*
   * Delay 1 ms to support intel controllers 
   * TODO: detect the subsequent intel controllers
   */
  delay(1000);

  /* Wait for cmd to clear */
  error = hcd->mmio_ops->mmio_wait_read(hcd, (10 * 1000 * 1000), &xhci->op_regs->cmd, XHCI_CMD_RESET, 0);

  if (error) return error;

  /* Wait for cnr to clear */
  error = hcd->mmio_ops->mmio_wait_read(hcd, (10 * 1000 * 1000), &xhci->op_regs->status, XHCI_STS_CNR, 0);

  if (error) return error;

  /* Clear interrupts */
  uint32_t stat = mmio_read_dword(&xhci->op_regs->status);
  mmio_write_dword(&xhci->op_regs->status, stat);
  mmio_write_dword(&xhci->op_regs->device_notif, 0);

  return 0;
}

static int xhci_force_halt(usb_hcd_t* hcd)
{
  xhci_hcd_t* xhci = hcd->private;

  uint32_t is_halted;
  uint32_t cmd_mask, cmd;

  /* Disable interrupts, engage halt */
  cmd_mask = ~(XHCI_IRQS);
  is_halted = mmio_read_dword(&xhci->op_regs->status) & XHCI_STS_HLT;

  /* The host controller is not yet halted, clear the run bit */
  if (!is_halted) {
    cmd_mask &= ~(XHCI_CMD_RUN);
    xhci->xhc_flags &= ~(XHC_FLAG_HALTED);
  }

  /* Write our halting command field */
  cmd = mmio_read_dword(&xhci->op_regs->cmd) & cmd_mask;
  mmio_write_dword(&xhci->op_regs->cmd, cmd);

  /* Wait for the host to mark itself halted */
  int error = hcd->mmio_ops->mmio_wait_read(hcd, XHCI_HALT_TIMEOUT_US, &xhci->op_regs->status, XHCI_STS_HLT, XHCI_STS_HLT);

  if (error)
    return error;

  /* Mark halted */
  xhci->xhc_flags |= XHC_FLAG_HALTED;

  return 0;
}

/*!
 * @brief Do HW specific host setup 
 *
 * allocate buffers, prepare stuff
 */
int xhci_setup(usb_hcd_t* hcd)
{
  int error;
  paddr_t xhci_register_p;
  size_t xhci_register_size;
  uint32_t bar0 = 0, bar1 = 0;

  /* Device pointers */
  pci_device_t* device = hcd->host_device;
  xhci_hcd_t* xhci = hcd->private;

  /* Enable the HC */
  pci_device_enable(device);

  device->ops.read_dword(device, BAR0, &bar0);
  device->ops.read_dword(device, BAR1, &bar1);

  xhci_register_size = pci_get_bar_size(device, 0);
  xhci_register_p = get_bar_address(bar0);

  if (is_bar_64bit(bar0))
    xhci_register_p |= get_bar_address(bar1) << 32;

  /*
   * Prepare by setting the register offsets 
   * we can use both the generic mmio functions
   * or the xhci hcd io functions to access these 
   * registers
   *
   * TODO: collect allocations by the xhci driver and usb subsystem so we can manage
   * them better lmao
   */
  xhci->register_ptr = Must(__kmem_kernel_alloc(
      xhci_register_p, xhci_register_size, NULL, KMEM_FLAG_KERNEL | KMEM_FLAG_DMA));

  xhci->cap_regs_offset = 0;
  xhci->cap_regs = (xhci_cap_regs_t*)xhci->register_ptr;

  xhci->oper_regs_offset = HC_LENGTH(xhci_read32(hcd, 0));
  xhci->op_regs = (xhci_op_regs_t*)(xhci->register_ptr + XHCI_OP_OF(xhci));

  xhci->runtime_regs_offset = xhci_read32(hcd, GET_OFFSET(xhci_cap_regs_t, runtime_regs_offset));
  xhci->runtime_regs = (xhci_runtime_regs_t*)(xhci->register_ptr + XHCI_RR_OF(xhci));

  xhci->hci_version = HC_VERSION(mmio_read_dword(&xhci->cap_regs->hc_capbase));
  xhci->max_interrupters = HC_MAX_INTER(mmio_read_dword(&xhci->cap_regs->hcc_params_1));

  uint32_t cap_len = HC_LENGTH(xhci_read32(hcd, 0));

  println_kterm("XHCI cap len: ");
  println_kterm(to_string(cap_len));
  println_kterm("XHCI version: ");
  println_kterm(to_string(xhci->hci_version));

  error = xhci_force_halt(hcd);

  if (error)
    return -1;

  println_kterm("Success halt: ");
  println_kterm(to_string(error));

  error = xhci_reset(hcd);

  println_kterm("Success reset: ");
  println_kterm(to_string(error));

  if (error)
    return -1;

  error = xhci_prepare_memory(hcd);

  println_kterm("Memory status: ");
  println_kterm(to_string(error));

  if (error)
    return -1;

  /*
   * TODO: support device quirks 
   *
   * It seems like no company can make a xhci host controller that is fully compliant 
   * with the xhci spec. This really powers up the need to support quirck registering 
   * per device. This is also an extra argument for creating a global device structure
   * (which was the plan anyways so yay) that can handle common device occurances. 
   * The device structure itself won't enforce how the quick bits are layed out and will
   * leave this to the underlying device
   */

  //kernel_panic(to_string(xhci_register_size));

  return 0;
}

/*!
 * @brief Start the hcd so it can handle IO requests
 *
 * Nothing to add here...
 */
int xhci_start(usb_hcd_t* hcd)
{
  int error;
  xhci_hcd_t* xhci = hcd->private;

  /* FIXME: dont fuck up the entire cmd register */
  //mmio_write_dword(&xhci->op_regs->cmd, XHCI_CMD_RUN | XHCI_CMD_HSEIE);

  error = hcd->mmio_ops->mmio_wait_read(hcd, XHCI_HALT_TIMEOUT_US, &xhci->op_regs->status, XHCI_STS_HLT, 0);

  println_kterm(to_string(error));
  kernel_panic("TODO: start xhci hcd");
  return 0;
}

/*!
 * @brief Stop the hcd so IO requests will be rejected
 *
 * Nothing to add here...
 */
void xhci_stop(usb_hcd_t* hcd)
{
  kernel_panic("TODO: stop xhci hcd");
  (void)hcd;
}

usb_hcd_hw_ops_t xhci_hw_ops = {
  .hcd_setup = xhci_setup,
  .hcd_start = xhci_start,
  .hcd_stop = xhci_stop,
};

static xhci_hcd_t* create_xhci_hcd()
{
  xhci_hcd_t* hub;

  hub = kmalloc(sizeof(xhci_hcd_t));

  if (!hub)
    return nullptr;

  memset(hub, 0, sizeof(*hub));

  return hub;
}

int xhci_probe(pci_device_t* device, pci_driver_t* driver)
{
  int error;
  usb_hcd_t* hcd;
  xhci_hcd_t* xhci_hcd;

  println_kterm("Probing for XHCI");

  hcd = create_usb_hcd(device, nullptr, USB_HUB_TYPE_XHCI);
  xhci_hcd = create_xhci_hcd();

  /* Set the hcd methods manually */
  hcd->private = xhci_hcd;
  hcd->mmio_ops = &xhci_mmio_ops;
  hcd->hw_ops = &xhci_hw_ops;

  error = register_usb_hcd(hcd);

  /* FUCKK */
  if (error)
    return error;

  return 0;
}

int xhci_remove(pci_device_t* device)
{
  pci_driver_t* driver = device->driver;

  /* Huh, thats weird? */
  if (!driver)
    return -1;

  usb_hcd_t* hcd = get_hcd_for_pci_device(device);

  if (!hcd)
    return -1;

  /* TODO: destroy this hcd and unregister it */
  kernel_panic("TODO: implement xhci_remove");

  xhci_hcd_t* xhci = hcd->private;

  xhci_destroy_scratchpad(xhci);

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
