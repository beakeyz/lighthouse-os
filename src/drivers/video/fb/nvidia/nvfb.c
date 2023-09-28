#include "dev/core.h"
#include "dev/driver.h"
#include "dev/pci/definitions.h"
#include "dev/precedence.h"
#include "dev/video/device.h"
#include "libk/flow/error.h"
#include <dev/pci/pci.h>

int nvfb_init();
int nvfb_exit();
uint64_t nvfb_msg(aniva_driver_t* this, dcc_t code, void* buffer, size_t size, void* out_buffer, size_t out_size);
int nvfb_probe(pci_device_t* dev, pci_driver_t* driver);

/*
 * A framebuffer driver for nvidia cards, inspiration from 
 * https://github.com/torvalds/linux/blob/master/drivers/video/fbdev/nvidia/nvidia.c
 *
 * This driver was probably targeted for older nvidia cards, hence it being in the linux kernel for at least 18 years,
 * but it seems like nvidia still supports this kind of driver? Or rather this kind of driver still supports more modern
 * nvidia cards? Otherwise this driver is for basically the first nvidia cards from 1999 or something. Not that these 
 * beasts will probably ever be found on a modern, UEFI-capable motherboard, but still. Who knows
 */

static pci_dev_id_t nv_ids[] = {
  PCI_DEVID(0x10de, 0, 0, 0, DISPLAY_CONTROLLER, 0, 0, PCI_DEVID_USE_VENDOR_ID | PCI_DEVID_USE_CLASS),
  PCI_DEVID_END,
};

static video_device_t nvfb_device = {
  .flags = VIDDEV_FLAG_FB,
  .max_connector_count = 1,
  .current_connector_count = 0,
  .ops = NULL,
};

pci_driver_t nvfb_pci_driver = {
  .id_table = nv_ids,
  .f_probe = nvfb_probe,
  .device_flags = NULL,
  0,
};

EXPORT_DRIVER(nvfb_driver) = {
  .m_name = "nvfb",
  .m_type = DT_GRAPHICS,
  .f_msg = nvfb_msg,
  .f_init = nvfb_init,
  .f_exit = nvfb_exit,
  .m_precedence = DRV_PRECEDENCE_BASIC,
  .m_dep_count = 0,
  .m_dependencies = { 0 },
};

int nvfb_probe(pci_device_t* dev, pci_driver_t* driver)
{
  register_video_device(&nvfb_driver, &nvfb_device);

  try_activate_video_device(&nvfb_device);
  return 0;
}

uint64_t nvfb_msg(aniva_driver_t* this, dcc_t code, void* buffer, size_t size, void* out_buffer, size_t out_size)
{
  return 0;
}

int nvfb_init() 
{
  register_pci_driver(&nvfb_pci_driver);
  return 0;
}

int nvfb_exit()
{
  unregister_pci_driver(&nvfb_pci_driver);
  return 0;
}
