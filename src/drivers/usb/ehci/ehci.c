#include "drivers/usb/ehci/ehci.h"
#include "dev/pci/definitions.h"
#include "dev/pci/pci.h"
#include "dev/usb/hcd.h"
#include "dev/usb/usb.h"
#include "dev/usb/xfer.h"
#include "drivers/usb/ehci/ehci_spec.h"
#include "libk/data/linkedlist.h"
#include "libk/flow/error.h"
#include "libk/io.h"
#include "mem/heap.h"
#include "mem/kmem_manager.h"
#include "mem/zalloc.h"
#include "proc/core.h"
#include "sched/scheduler.h"
#include "sync/mutex.h"
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

enum EHCI_INTERRUPT_STS {
  USBINT = 0,
  USBERRINT,
  PORTCHANGE,
  FLROLLOVER,
  HOSTSYSERR,
  INTONAA,
};

int ehci_get_port_sts(ehci_hcd_t* ehci, uint32_t port, usb_port_status_t* status)
{
  uint32_t portsts;

  if (port >= ehci->portcount)
    return -1;

  portsts = mmio_read_dword(ehci->opregs + EHCI_OPREG_PORTSC + (port * sizeof(uint32_t)));

  memset(status, 0, sizeof(*status));

  /* Now we need to translate EHCI status to generic USB status =/ */
  if (portsts & EHCI_PORTSC_CONNECT)
    status->status |= USB_PORT_STATUS_CONNECTION;
  if (portsts & EHCI_PORTSC_ENABLE)
    status->status |= (USB_PORT_STATUS_ENABLE | USB_PORT_STATUS_HIGH_SPEED);
  if (portsts & EHCI_PORTSC_OVERCURRENT)
    status->status |= USB_PORT_STATUS_OVER_CURRENT;
  if (portsts & EHCI_PORTSC_RESET)
    status->status |= USB_PORT_STATUS_RESET;
  if (portsts & EHCI_PORTSC_PORT_POWER)
    status->status |= USB_PORT_STATUS_POWER;
  if (portsts & EHCI_PORTSC_SUSPEND)
    status->status |= USB_PORT_STATUS_SUSPEND;
  if (portsts & EHCI_PORTSC_DMINUS_LINESTAT)
    status->status |= USB_PORT_STATUS_LOW_SPEED;

  if ((portsts & EHCI_PORTSC_CONNECT_CHANGE) == EHCI_PORTSC_CONNECT_CHANGE)
    status->change |= USB_PORT_STATUS_CONNECTION;
  if ((portsts & EHCI_PORTSC_ENABLE_CHANGE) == EHCI_PORTSC_ENABLE_CHANGE)
    status->change |= USB_PORT_STATUS_ENABLE;
  if ((portsts & EHCI_PORTSC_OVERCURRENT_CHANGE) == EHCI_PORTSC_OVERCURRENT_CHANGE)
    status->change |= USB_PORT_STATUS_OVER_CURRENT;

  /* TODO: Reset change and suspend change */

  return 0;
}

int ehci_set_port_feature(ehci_hcd_t* ehci, uint32_t port, uint16_t feature)
{
  kernel_panic("TODO: ehci_set_port_feature");
}

int ehci_clear_port_feature(ehci_hcd_t* ehci, uint32_t port, uint16_t feature)
{
  uint32_t port_sts;

  if (port >= ehci->portcount)
    return -KERR_INVAL;

  port_sts = mmio_read_dword(ehci->opregs + EHCI_OPREG_PORTSC + (port * sizeof(uint32_t)));

  switch (feature) {
    case USB_FEATURE_PORT_ENABLE:
      port_sts &= ~(EHCI_PORTSC_ENABLE);
      break;
    case USB_FEATURE_PORT_POWER:
      port_sts &= ~(EHCI_PORTSC_PORT_POWER);
      break;
    case USB_FEATURE_C_PORT_ENABLE:
      port_sts |= EHCI_PORTSC_ENABLE_CHANGE;
      break;
    case USB_FEATURE_C_PORT_CONNECTION:
      port_sts |= EHCI_PORTSC_CONNECT_CHANGE;
      break;
    case USB_FEATURE_C_PORT_OVER_CURRENT:
      port_sts |= EHCI_PORTSC_OVERCURRENT_CHANGE;
      break;
  }

  mmio_write_dword(ehci->opregs + EHCI_OPREG_PORTSC + (port * sizeof(uint32_t)), port_sts);
  return 0;
}

static int ehci_interrupt_poll(ehci_hcd_t* ehci)
{
  uint32_t usbsts;
  printf("EHCI: entered the ehci polling routine\n");

  while ((ehci->ehci_flags & EHCI_HCD_FLAG_STOPPING) != EHCI_HCD_FLAG_STOPPING) {
    usbsts = mmio_read_dword(ehci->opregs + EHCI_OPREG_USBSTS) & 0x3f;

    for (uint32_t i = 0; i < 6; i++) {
      if ((usbsts & (1 << i)) != (1 << i))
        continue;

      switch (i) {
        case USBINT:
          printf("EHCI: Finished a transfer!\n");
          break;
        case USBERRINT:
          printf("EHCI: Got a transfer error!\n");
          break;
        case PORTCHANGE:
          printf("EHCI: Port change occured!\n");
          break;
        case FLROLLOVER:
          printf("EHCI: Frame List rollover!\n");
          break;
        case HOSTSYSERR:
          printf("EHCI: Host system error (yikes)!\n");
          break;
        case INTONAA:
          printf("EHCI: Advanced the async qh!\n");
          break;
      }

      /* Clear the bit */
      //usbsts &= ~(1 << i);
    }

    mmio_write_dword(ehci->opregs + EHCI_OPREG_USBSTS, usbsts);

    scheduler_yield();
  }
  return 0;
}

static int ehci_transfer_finish(ehci_hcd_t* ehci)
{
  ehci_qtd_t* c_qtd;
  ehci_xfer_t* c_xfer;

  printf("EHCI: entered the ehci transfer finish routine\n");

  while ((ehci->ehci_flags & EHCI_HCD_FLAG_STOPPING) != EHCI_HCD_FLAG_STOPPING) {

    /* Spin until there are transfers to process */
    while (ehci->transfer_list->m_length == 0)
      scheduler_yield();

    mutex_lock(ehci->transfer_lock);

    FOREACH(i, ehci->transfer_list) {
      c_xfer = i->data;
      c_qtd = c_xfer->qh->qtd_link;

      do {
        if (c_qtd->hw_token & EHCI_QTD_STATUS_ACTIVE)
          break;

        printf("QTD 0x%x inactive!\n", c_qtd->hw_token);
        c_qtd = c_qtd->next;
      } while(c_qtd);
    }

    mutex_unlock(ehci->transfer_lock);

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

static int ehci_halt(ehci_hcd_t* ehci)
{
  uint32_t tmp;

  /* Make sure we don't generate shit */
  mmio_write_dword(ehci->opregs + EHCI_OPREG_USBINTR, 0);

  tmp = mmio_read_dword(ehci->opregs + EHCI_OPREG_USBCMD);
  tmp &= ~(EHCI_OPREG_USBCMD_RS | EHCI_OPREG_USBCMD_INT_ON_ASYNC_ADVANCE_DB);

  mmio_write_dword(ehci->opregs + EHCI_OPREG_USBCMD, tmp);

  /* Wait a bit until the ehci is halted */
  for (uint32_t i = 0; i < 16; i++) {
    tmp = mmio_read_dword(ehci->opregs + EHCI_OPREG_USBSTS);

    if ((tmp & EHCI_OPREG_USBSTS_HCHALTED) == EHCI_OPREG_USBSTS_HCHALTED)
      return 0;

    mdelay(125);
  }

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

  ehci->async = create_ehci_qh(ehci, NULL);

  ehci->async->hw_next = ehci->async->qh_dma;
  ehci->async->next = ehci->async;
  ehci->async->prev = ehci->async;
  
  ehci->async->hw_info_0 = EHCI_QH_HEAD | EHCI_QH_INACTIVATE;
  ehci->async->hw_qtd_token = EHCI_QTD_STATUS_HALTED;
  ehci->async->hw_qtd_next = EHCI_FLLP_TYPE_END;

  printf("Writing async list addr: %llx\n", ehci->async->qh_dma);

  /* Set the async pointer */
  mmio_write_dword(ehci->opregs + EHCI_OPREG_ASYNCLISTADDR, ehci->async->qh_dma);

  return 0;
}

static int ehci_init_interrupts(ehci_hcd_t* ehci)
{
  /* TODO: Setup actual interrupts */
  ehci->interrupt_polling_thread = spawn_thread("EHCI Polling", (FuncPtr)ehci_interrupt_poll, (uint64_t)ehci);

  ASSERT_MSG(ehci->interrupt_polling_thread, "Failed to spawn the EHCI Polling thread!");

  /* Interrupts are enabled in ehci_start */
  return 0;
}

static int ehci_setup(usb_hcd_t* hcd)
{
  int error;
  ehci_hcd_t* ehci = hcd->private;
  uintptr_t bar0 = 0;
  uint8_t release_num, caplen;

  hcd->pci_device->ops.read_dword(hcd->pci_device, BAR0, (uint32_t*)&bar0);

  ehci->register_size = pci_get_bar_size(hcd->pci_device, 0); ehci->capregs = (void*)Must(__kmem_kernel_alloc(get_bar_address(bar0), ehci->register_size, NULL, KMEM_FLAG_DMA));

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

  printf("Reseting EHCI\n");

  /* Reset ourselves */
  error = ehci_reset(ehci);

  if (error)
    return error;

  /* Spec wants us to set the segment register */
  mmio_write_dword(ehci->opregs + EHCI_OPREG_CTRLDSSEGMENT, 0);

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

  error = ehci_halt(ehci);

  if (error)
    return error;

  ehci->transfer_list = init_list();
  ehci->transfer_lock = create_mutex(NULL);
  ehci->transfer_finish_thread = spawn_thread("EHCI Transfer Finisher", (FuncPtr)ehci_transfer_finish, (uintptr_t)ehci);

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
  c_tmp &= ~(
      EHCI_OPREG_USBCMD_HC_RESET | EHCI_OPREG_USBCMD_INT_ON_ASYNC_ADVANCE_DB | 
      EHCI_OPREG_USBCMD_LHC_RESET | EHCI_OPREG_USBCMD_ASYNC_SCHEDULE_ENABLE | EHCI_OPREG_USBCMD_PERIODIC_SCHEDULE_ENABLE);
  c_tmp |= EHCI_OPREG_USBCMD_RS;

  mmio_write_dword(ehci->opregs + EHCI_OPREG_USBCMD, c_tmp);

  /* Wait for the spinup to finish */
  for (c_hcstart_spin = 0; c_hcstart_spin < EHCI_SPINUP_LIMIT; c_hcstart_spin++) {
    c_tmp = mmio_read_dword(ehci->opregs + EHCI_OPREG_USBSTS);

    if ((c_tmp & EHCI_OPREG_USBSTS_HCHALTED) == 0)
      break;

    mdelay(500);
  }

  if (c_hcstart_spin == EHCI_SPINUP_LIMIT) {
    /* Failed to start, yikes */
    printf("EHCI: Failed to start\n");
    return -1;
  }

  c_tmp = mmio_read_dword(ehci->opregs + EHCI_OPREG_USBCMD);
  /* Clear interrupt ctl */
  c_tmp &= ~(EHCI_OPREG_USBCMD_INTER_CTL(EHCI_OPREG_USBCMD_INTER_CTL_MASK) | EHCI_OPREG_USBCMD_PPCEE);
  /* Program framelist size */
  c_tmp |= EHCI_OPREG_USBCMD_FRAMELIST_SIZE(((c_tmp >> EHCI_OPREG_USBCMD_FRAMELIST_SIZE_SHIFT) & EHCI_OPREG_USBCMD_FRAMELIST_SIZE_MASK)));
  /* Program interrupt ctl */
  c_tmp |= (1 << EHCI_OPREG_USBCMD_INTER_CTL_SHIFT);

  mmio_write_dword(ehci->opregs + EHCI_OPREG_USBCMD, c_tmp);

  /* 
   * Route ports to us from any companion controller(s)
   * NOTE/FIXME: These controllers may be in the middle of a port reset. This 
   * may cause some issues...
   * See: https://github.com/torvalds/linux/blob/master/drivers/usb/host/ehci-hcd.c#L573
   */
  mmio_write_dword(ehci->opregs + EHCI_OPREG_CONFIGFLAG, 1);

  /* Flush */
  c_tmp = mmio_read_dword(ehci->opregs + EHCI_OPREG_USBCMD);
  mdelay(5);

  ehci->cur_interrupt_state = 
      EHCI_USBINTR_HOSTSYSERR | EHCI_USBINTR_USBERRINT |
      EHCI_USBINTR_INTONAA | EHCI_USBINTR_PORTCHANGE | 
      EHCI_USBINTR_USBINT;

  /* Write the things to the HC */
  mmio_write_dword(ehci->opregs + EHCI_OPREG_USBINTR, ehci->cur_interrupt_state);

  /* Create this roothub */
  hcd->roothub = create_usb_hub(hcd, USB_HUB_TYPE_EHCI, nullptr, 0, 0, ehci->portcount);

  /* Enumerate the hub */
  usb_hub_enumerate(hcd->roothub);

  printf("Started the EHCI controller\n");
  return 0;
}

static void ehci_stop(usb_hcd_t* hcd)
{
  (void)hcd;
  kernel_panic("ehci_stop");
}

usb_hcd_hw_ops_t ehci_hw_ops = {
  .hcd_setup = ehci_setup,
  .hcd_start = ehci_start,
  .hcd_stop = ehci_stop,
};

static inline void _ehci_try_enable_async(ehci_hcd_t* ehci)
{
  uint32_t cmd;
  uint32_t sts;

  sts = mmio_read_dword(ehci->opregs + EHCI_OPREG_USBSTS);

  /* Already enabled */
  if ((sts & EHCI_OPREG_USBSTS_ASSTATUS) == EHCI_OPREG_USBSTS_ASSTATUS)
    return;

  cmd = mmio_read_dword(ehci->opregs + EHCI_OPREG_USBCMD);

  cmd |= EHCI_OPREG_USBCMD_PERIODIC_SCHEDULE_ENABLE | EHCI_OPREG_USBCMD_INT_ON_ASYNC_ADVANCE_DB;
  mmio_write_dword(ehci->opregs, cmd);
}

/*!
 * @brief: Add a qh to the async list
 *
 * Places the new qh right before the current ehci->async
 */
static int _ehci_add_async_qh(ehci_hcd_t* ehci, ehci_qh_t* qh)
{
  //mutex_lock(ehci->queue_lock);

  qh->hw_next = ehci->async->qh_dma;
  qh->next= ehci->async;
  qh->prev = ehci->async->prev;

  ehci->async->prev = qh;

  qh->prev->next = qh;
  qh->prev->hw_next = qh->qh_dma;

  /* Maybe enable */
  _ehci_try_enable_async(ehci);

  //mutex_unlock(ehci->queue_lock);
  return 0;
}

int ehci_enqueue_transfer(usb_hcd_t* hcd, usb_xfer_t* xfer)
{
  int error;
  ehci_qh_t* qh;
  ehci_hcd_t* ehci;
  ehci_xfer_t* e_xfer;
  usb_hub_t* roothub;

  /*
   * This may happen when we're trying to create the EHCI roothub for this hcd. Should be okay
   */
  if (!hcd->roothub)
    return -1;

  ehci = hcd->private;

  roothub = hcd->roothub;

  if (xfer->req_devaddr == roothub->device->dev_addr)
    return ehci_process_hub_xfer(roothub, xfer);

  /* Process the EHCI transfer */
  qh = create_ehci_qh(ehci, xfer);

  switch(xfer->req_type) {
    case USB_CTL_XFER:
      error = ehci_init_ctl_queue(ehci, xfer, qh);
      break;
    default:
      error = ehci_init_data_queue(ehci, xfer, qh);
      break;
  }

  e_xfer = create_ehci_xfer(xfer, qh);

  if (!e_xfer)
    goto destroy_and_exit;

  /* Enqueue */
  ehci_enq_xfer(ehci, e_xfer);

  if (error)
    goto destroy_and_exit;

  error = _ehci_add_async_qh(ehci, qh);

  if (error)
    goto destroy_and_exit;

  return 0;
destroy_and_exit:
  destroy_ehci_qh(ehci, qh);
  return error;
}

usb_hcd_io_ops_t ehci_io_ops = {
  .enq_request = ehci_enqueue_transfer,
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

  hcd = create_usb_hcd(device, "ehci_hcd", NULL);

  if (!hcd)
    return -KERR_NOMEM;

  /* Create the bitch */
  ehci = create_ehci_hcd(hcd);

  if (!ehci)
    goto dealloc_and_exit;

  register_usb_hcd(hcd);
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
