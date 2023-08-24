#include "dev/pci/definitions.h"
#include <dev/pci/pci.h>

/*
 * A framebuffer driver for nvidia cards, inspiration from 
 * https://github.com/torvalds/linux/blob/master/drivers/video/fbdev/nvidia/nvidia.c
 *
 * This driver was probably targeted for older nvidia cards, hence it being in the linux kernel for at least 18 years,
 * but it seems like nvidia still supports this kind of driver? Or rather this kind of driver still supports more modern
 * nvidia cards? Otherwise this driver is for basically the first nvidia cards from 1999 or something. Not that these 
 * beasts will probably ever be found on a modern, UEFI-capable motherboard, but still. Who knows
 */

static const pci_dev_id_t nv_ids[] = {
  PCI_DEVID(0x10de, 0, 0, 0, DISPLAY_CONTROLLER, 0, 0, PCI_DEVID_USE_VENDOR_ID | PCI_DEVID_USE_CLASS),
  PCI_DEVID_END,
};

int nvfb_init() 
{
  (void)nv_ids;

  return 0;
}
