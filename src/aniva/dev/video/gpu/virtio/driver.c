
#include "dev/driver.h"
#include "dev/core.h"
#include "dev/precedence.h"
#include <dev/pci/pci.h>

/*
 * The VirtIO graphics device driver
 *
 * This driver serves as an example for myself of how to form the exact interface between
 * the userspace that wants to draw shit, and the kernel/driver space that wants to have an
 * easy time talking to hardware. I will simply try to write a functioning driver with the 
 * interfaces that I have right now and when I am missing something, I will add it to the 
 * video API of the kernel.
 *
 * Maybe we find that we are in fact able to write a fully functioning driver with what we 
 * have right now, but the only problem is that the userspace cant talk to us... then we'll
 * probably have to edit BOTH the driver structure AND the video API =/
 *
 * Yeesh
 */

int init_virtio();
int exit_virtio();

static pci_dev_id_t virtio_id_table[] = {
  PCI_DEVID_END,
};

int virtio_probe(pci_device_t* device, pci_driver_t* driver) {

  /* TODO: implement probing */
  return -1;
}

pci_driver_t virtio_pci_drv = {
  .f_probe = virtio_probe,
  .id_table = virtio_id_table,
  .device_flags = NULL,
};

/*
 * TODO: automatically unload this driver when we don't detect 
 * virtio
 */
aniva_driver_t virtio_drv = {
  .m_name = "virtio_drv",
  .m_precedence = DRV_PRECEDENCE_MID,
  .m_type = DT_GRAPHICS,
  .f_init = init_virtio,
  .f_exit = exit_virtio,
};

int init_virtio()
{
  /* This fails if there is no virtio device present on the system */
  return register_pci_driver(&virtio_pci_drv);
}

int exit_virtio()
{
  return 0;
}
