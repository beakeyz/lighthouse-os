#include "drivers/usb/ehci/ehci.h"
#include "dev/pci/definitions.h"
#include "dev/pci/pci.h"
#include "dev/usb/hcd.h"
#include "dev/usb/usb.h"
#include "drivers/usb/ehci/ehci_spec.h"
#include "libk/flow/error.h"
#include "libk/io.h"
#include "mem/heap.h"
#include "mem/kmem_manager.h"
#include "mem/zalloc.h"
#include "proc/core.h"
#include "sched/scheduler.h"
#include <dev/core.h>
#include <dev/manifest.h>

/*
 * EHCI HC driver
 *
 * A few notes:
 * 1) This driver does not take into account the endianness of the HC
 */

static pci_dev_id_t ehci_pci_ids[] = {
  PCI_DEVID_CLASSES(SERIAL_BUS_CONTROLLER, PCI_SUBCLASS_SBC_USB, PCI_PROGIF_EHCI),
  PCI_DEVID_END,
};

static int ehci_interrupt_poll(ehci_hcd_t* ehci)
{
  printf("EHCI: entered the ehci polling routine\n");

  while (true) {
    scheduler_yield();
  }
  return 0;
}

static int ehci_take_bios_ownership(ehci_hcd_t* ehci)
{
  uint32_t legsup;
  uint32_t takeover_tries;
  pci_device_t* dev;
  uint32_t ext_cap = EHCI_GETPARAM_VALUE(ehci->hcc_params, EHCI_HCCPARAMS_EECP, EHCI_HCCPARAMS_EECP_OFFSET);

  if (!ext_cap)
    return 0;

  dev = ehci->hcd->pci_device;

  if (dev->ops.read_dword(dev, ext_cap, &legsup))
    return -1;

  if ((legsup & EHCI_LEGSUP_CAPID_MASK) != EHCI_LEGSUP_CAPID)
    return -1;
  
  /* Not bios-owned, houray */
  if ((legsup & EHCI_LEGSUP_BIOSOWNED) == 0)
    return 0;

  /* First try */
  takeover_tries = 1;

  printf("We need to take control of EHCI from the BIOS (fuck)\n");

  /* Write a one into the OS-owned byte */
  pci_device_write8(dev, ext_cap + 3, 1);

  /* Keep reading the bios-owned byte */
  do {
    pci_device_read32(dev, ext_cap, &legsup);

    printf("Checking...\n");

    if ((legsup & EHCI_LEGSUP_BIOSOWNED) == 0)
      return 0;

    mdelay(50000);
  } while (takeover_tries++ < 16);

  if ((legsup & EHCI_LEGSUP_OSOWNED) == 1)
    return 0;

  return -1;
}

static int ehci_reset(ehci_hcd_t* ehci)
{
  uint32_t usbcmd;

  mmio_write_dword(ehci->opregs + EHCI_OPREG_USBCMD, 0);

  /* Sleep a bit until the reset has come through */
  mdelay(100);

  usbcmd = mmio_read_dword(ehci->opregs + EHCI_OPREG_USBCMD);
  usbcmd |= EHCI_OPREG_USBCMD_HC_RESET;
  mmio_write_dword(ehci->opregs + EHCI_OPREG_USBCMD, usbcmd);

  for (uint32_t i = 0; i < 8; i++) {
    usbcmd = mmio_read_dword(ehci->opregs + EHCI_OPREG_USBCMD);

    if ((usbcmd & EHCI_OPREG_USBCMD_HC_RESET) == 0)
      return 0;

    mdelay(50);
  }

  return -1;
}

/*!
 * @brief: Allocate the structures needed for the function of ehci
 *
 */
static int ehci_init_mem(ehci_hcd_t* ehci)
{
  /* Alignment should be good */
  ehci->qtd_pool = create_zone_allocator(4096, sizeof(ehci_qtd_t), ZALLOC_FLAG_DMA);

  /* Alignment should also be good */
  ehci->qh_pool = create_zone_allocator(4096, sizeof(ehci_qh_t), ZALLOC_FLAG_DMA);

  /* Alignment should also be good */
  ehci->itd_pool = create_zone_allocator(4096, sizeof(ehci_itd_t), ZALLOC_FLAG_DMA);

  /* Alignment should also be good */
  ehci->sitd_pool = create_zone_allocator(4096, sizeof(ehci_sitd_t), ZALLOC_FLAG_DMA);

  /* Allocate the periodic shedule table */
  ehci->periodic_table = (uint32_t*)Must(__kmem_kernel_alloc_range(
        ehci->periodic_size * sizeof(uint32_t),
        NULL, KMEM_FLAG_DMA));
  ehci->periodic_dma = kmem_to_phys(nullptr, (vaddr_t)ehci->periodic_table);

  /* Make sure to terminate all the pariodic frame entries */
  for (uint32_t i = 0; i < ehci->periodic_size; i++)
    ehci->periodic_table[i] = EHCI_FLLP_TYPE_END;

  /* Write the thing lmao */
  mmio_write_dword(ehci->opregs + EHCI_OPREG_PERIODICLISTBASE, ehci->periodic_dma);

  return 0;
}

static int ehci_init_interrupts(ehci_hcd_t* ehci)
{
  /* TODO: Setup actual interrupts */
  ehci->interrupt_polling_thread = spawn_thread("EHCI Polling", (FuncPtr)ehci_interrupt_poll, (uint64_t)ehci);

  ASSERT_MSG(ehci->interrupt_polling_thread, "Failed to spawn the EHCI Polling thread!");

  ehci->cur_interrupt_state = 
      EHCI_USBINTR_HOSTSYSERR | EHCI_USBINTR_USBERRINT |
      EHCI_USBINTR_INTONAA | EHCI_USBINTR_PORTCHANGE | 
      EHCI_USBINTR_USBINT;

  /* Write the things to the HC */
  mmio_write_dword(ehci->opregs + EHCI_OPREG_USBINTR, ehci->cur_interrupt_state);

  return 0;
}

static int ehci_setup(usb_hcd_t* hcd)
{
  int error;
  ehci_hcd_t* ehci = hcd->private;
  uintptr_t bar0 = 0;
  uint8_t release_num, caplen;

  hcd->pci_device->ops.read_dword(hcd->pci_device, BAR0, (uint32_t*)&bar0);

  ehci->register_size = pci_get_bar_size(hcd->pci_device, 0);
  ehci->capregs = (void*)Must(__kmem_kernel_alloc(get_bar_address(bar0), ehci->register_size, NULL, NULL));

  printf("Setup EHCI registerspace (addr=0x%p, size=0x%llx)\n", ehci->capregs, ehci->register_size);

  hcd->pci_device->ops.read_byte(hcd->pci_device, EHCI_PCI_SBRN, &release_num);

  printf("EHCI release number: 0x%x\n", release_num);

  /* Compute the opregs address */
  caplen = mmio_read_byte(ehci->capregs + EHCI_HCCR_CAPLENGTH);
  ehci->opregs = ehci->capregs + caplen;

  printf("EHCI opregs at 0x%p (caplen=0x%x)\n", ehci->opregs, caplen);

  ehci->hcs_params = mmio_read_dword(ehci->capregs + EHCI_HCCR_HCSPARAMS);
  ehci->hcc_params = mmio_read_dword(ehci->capregs + EHCI_HCCR_HCCPARAMS);
  ehci->portcount = EHCI_GETPARAM_VALUE(ehci->hcs_params, EHCI_HCSPARAMS_N_PORTS, EHCI_HCSPARAMS_N_PORTS_OFFSET) & 0x0f;

  /* Default periodic size (TODO: this may be smaller than the default, detect this) */
  ehci->periodic_size = 1024;

  printf("EHCI HCD portcount: %d\n", ehci->portcount);

  /* Make sure we own this biatch */
  error = ehci_take_bios_ownership(ehci);

  if (error)
    return error;

  /* Make sure we don't generate shit */
  mmio_write_dword(ehci->opregs + EHCI_OPREG_USBINTR, 0);

  printf("Reseting EHCI\n");

  /* Reset ourselves */
  error = ehci_reset(ehci);

  if (error)
    return error;

  /* Spec wants us to set the segment register */
  mmio_write_dword(ehci->opregs + EHCI_OPREG_CTRLDSSEGMENT, 0);

  /* TODO: Further init following the EHCI spec
   */

  printf("Doing EHCI memory\n");

  /* Initialize EHCI memory */
  error = ehci_init_mem(ehci);

  if (error)
    return error;

  printf("Doing EHCI interrupts\n");

  /* Initialize EHCI interrupt stuff */
  error = ehci_init_interrupts(ehci);

  if (error)
    return error;

  printf("Done with EHCI initialization\n");
  return 0;
}

static int ehci_start(usb_hcd_t* hcd)
{
  uint32_t c_hcstart_spin;
  uint32_t c_tmp;
  ehci_hcd_t* ehci;
  printf("Starting ECHI controller!\n");

  ehci = hcd->private;

  c_tmp = mmio_read_dword(ehci->opregs + EHCI_OPREG_USBCMD);
  c_tmp &= ~(EHCI_OPREG_USBCMD_INTER_CTL | EHCI_OPREG_USBCMD_HC_RESET | EHCI_OPREG_USBCMD_LHC_RESET);
  c_tmp &= ~(EHCI_OPREG_USBCMD_ASYNC_SCHEDULE_ENABLE | EHCI_OPREG_USBCMD_PERIODIC_SCHEDULE_ENABLE);

  mmio_write_dword(ehci->opregs + EHCI_OPREG_USBCMD, 
      c_tmp | EHCI_OPREG_USBCMD_RS | EHCI_OPREG_USBCMD_ASPM_ENABLE |
      /* NOTE: Interrupt configuration */
      (1 << 16)
      );

  /* Wait for the spinup to finish */
  for (c_hcstart_spin = 0; c_hcstart_spin < EHCI_SPINUP_LIMIT; c_hcstart_spin++) {
    c_tmp = mmio_read_dword(ehci->opregs + EHCI_OPREG_USBSTS);

    if ((c_tmp & EHCI_OPREG_USBSTS_HCHALTED) != EHCI_OPREG_USBSTS_HCHALTED)
      break;

    mdelay(500);
  }

  if (c_hcstart_spin == EHCI_SPINUP_LIMIT) {
    /* Failed to start, yikes */
    printf("EHCI: Failed to start\n");
    return -1;
  }

  /* 
   * Route ports to us from any companion controller(s)
   * NOTE/FIXME: These controllers may be in the middle of a port reset. This 
   * may cause some issues...
   */
  mmio_write_dword(ehci->opregs + EHCI_OPREG_CONFIGFLAG, 1);

  /* Flush */
  c_tmp = mmio_read_dword(ehci->opregs + EHCI_OPREG_USBCMD);
  mdelay(5);

  printf("Started the EHCI controller\n");
  return 0;
}

static void ehci_stop(usb_hcd_t* hcd)
{
  (void)hcd;
}

usb_hcd_hw_ops_t ehci_hw_ops = {
  .hcd_setup = ehci_setup,
  .hcd_start = ehci_start,
  .hcd_stop = ehci_stop,
};

usb_hcd_io_ops_t ehci_io_ops = {
  NULL,
};

static ehci_hcd_t* create_ehci_hcd(usb_hcd_t* hcd)
{
  ehci_hcd_t* ehci;

  ehci = kmalloc(sizeof(*ehci));

  if (!ehci)
    return nullptr;

  memset(ehci, 0, sizeof(*ehci));

  ehci->hcd = hcd;

  hcd->private = ehci;
  hcd->hw_ops = &ehci_hw_ops;
  hcd->io_ops = &ehci_io_ops;

  return ehci;
}

/*!
 * @brief: Probe function of the EHCI PCI driver
 *
 * Called when the PCI bus scan finds a device matching our devid list
 */
int ehci_probe(pci_device_t* device, pci_driver_t* driver)
{
  ehci_hcd_t* ehci;
  usb_hcd_t* hcd;

  printf("Found a EHCI device! {\n\tvendorid=0x%x,\n\tdevid=0x%x,\n\tclass=0x%x,\n\tsubclass=0x%x\n}\n", device->vendor_id, device->dev_id, device->class, device->subclass);

  /* Enable the PCI device */
  pci_device_enable(device);

  hcd = create_usb_hcd(device, "ehci_hcd", USB_HUB_TYPE_EHCI, NULL);

  if (!hcd)
    return -KERR_NOMEM;

  /* Create the bitch */
  ehci = create_ehci_hcd(hcd);

  if (!ehci)
    goto dealloc_and_exit;

  register_usb_hcd(hcd);

  kernel_panic("EHCI");
  return 0;

dealloc_and_exit:
  destroy_usb_hcd(hcd);
  return -KERR_INVAL;
}

int ehci_remove(pci_device_t* device)
{
  return 0;
}

/*
 * xhci is hosted through the PCI bus, so we'll register it through that
 */
pci_driver_t ehci_pci_driver = {
  .f_probe = ehci_probe,
  .f_pci_on_remove = ehci_remove,
  .id_table = ehci_pci_ids,
  .device_flags = NULL,
};

int ehci_init(drv_manifest_t* this)
{
  register_pci_driver(&ehci_pci_driver);
  return 0;
}

int ehci_exit()
{
  unregister_pci_driver(&ehci_pci_driver);
  return 0;
}

EXPORT_DRIVER(ehci_hcd) = {
  .m_name = "ehci",
  .m_descriptor = "EHCI host controller driver",
  .m_type = DT_IO,
  .m_version = DRIVER_VERSION(0, 0, 1),
  .f_init = ehci_init,
  .f_exit = ehci_exit,
};

/* No dependencies */
EXPORT_DEPENDENCIES(deps) = {
  DRV_DEP_END,
};
