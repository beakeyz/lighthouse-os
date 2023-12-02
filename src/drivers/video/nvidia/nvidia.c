#include "dev/core.h"
#include "dev/driver.h"
#include "dev/pci/definitions.h"
#include "dev/precedence.h"
#include "dev/video/device.h"
#include "drivers/video/nvidia/device/device.h"
#include "libk/flow/error.h"
#include "mem/heap.h"
#include <dev/pci/pci.h>
#include <dev/manifest.h>

static dev_manifest_t* _our_manifest;

int nvidia_init();
int nvidia_exit();
uint64_t nvidia_msg(aniva_driver_t* this, dcc_t code, void* buffer, size_t size, void* out_buffer, size_t out_size);
int nvidia_probe(pci_device_t* dev, pci_driver_t* driver);

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
  PCI_DEVID(0x12d2, 0, 0, 0, DISPLAY_CONTROLLER, 0, 0, PCI_DEVID_USE_VENDOR_ID | PCI_DEVID_USE_CLASS),
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
  .f_probe = nvidia_probe,
  .device_flags = NULL,
  0,
};

EXPORT_DRIVER(nvidia_driver) = {
  .m_name = "nvidia",
  .m_type = DT_GRAPHICS,
  .f_msg = nvidia_msg,
  .f_init = nvidia_init,
  .f_exit = nvidia_exit,
  .m_precedence = DRV_PRECEDENCE_BASIC,
  .m_dep_count = 0,
  .m_dependencies = { 0 },
};

/*!
 * @brief: Main detection entry for the nvidia kernel driver
 *
 * When we detect a nvidia card on the PCI bus, this gets called. Here, we are responsible for asserting that we 
 * support the device we are given to the extent that we say we can to the video core.
 */
int nvidia_probe(pci_device_t* dev, pci_driver_t* driver)
{
  nv_device_t* nvdev;

  logln("Found an NVIDIA device!");

  /* Yikes =/ */
  if (video_deactivate_current_driver())
    return -1;

  /* Try to create and initialize the card */
  nvdev = create_nv_device(&nvidia_driver, dev);

  ASSERT(nvdev);

  log("PRI addr: ");
  logln(to_string((uintptr_t)nvdev->pri));
  log("PRI size: ");
  logln(to_string(nvdev->pri_size));
  log("NV card type: ");
  logln(to_string(nvdev->card_type));
  log("NV chipset: ");
  logln(to_string(nvdev->chipset));
  log("NV chiprev: ");
  logln(to_string(nvdev->chiprev));
  log("NV vendor: ");
  logln(to_string(nvdev->id.vendor));
  log("NV device: ");
  logln(to_string(nvdev->id.device));

  /* TODO: remove standard boot-time video drivers like efi */
  register_video_device(&nvfb_device);
  return 0;
}

uint64_t nvidia_msg(aniva_driver_t* this, dcc_t code, void* buffer, size_t size, void* out_buffer, size_t out_size)
{
  return 0;
}

/*!
 * @brief: nvidia driver initialization
 *
 * We need to expose our manifest to the rest of the driver, since we need it to
 * attach our graphics device
 */
int nvidia_init() 
{
  /* We should be active at this point lol */
  _our_manifest = try_driver_get(&nvidia_driver, NULL);

  if (!_our_manifest)
    return -1;

  register_pci_driver(&nvfb_pci_driver);
  return 0;
}

int nvidia_exit()
{
  unregister_pci_driver(&nvfb_pci_driver);
  return 0;
}
