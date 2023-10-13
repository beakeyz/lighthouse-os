#include "dev/core.h"
#include "dev/debug/serial.h"
#include "dev/driver.h"
#include "dev/pci/pci.h"
#include "dev/precedence.h"
#include "dev/video/device.h"
#include "libk/flow/error.h"
#include "ids.h"

pci_dev_id_t radeon_id_table[] = {
  PCI_DEVID_IDS_EX(PCI_VENDOR_AMD, PCI_DEVICE_R200_GL, 0, 0, PCI_DEVID_USE_VENDOR_ID | PCI_DEVID_USE_DEVICE_ID),
  PCI_DEVID_IDS_EX(PCI_VENDOR_AMD, PCI_DEVICE_R200_RAD8500, 0, 0, PCI_DEVID_USE_VENDOR_ID | PCI_DEVID_USE_DEVICE_ID),
  PCI_DEVID_IDS_EX(PCI_VENDOR_AMD, PCI_DEVICE_R200_RAD9100, 0, 0, PCI_DEVID_USE_VENDOR_ID | PCI_DEVID_USE_DEVICE_ID),
  PCI_DEVID_END,
};

int radeon_init();
int radeon_exit();
uintptr_t radeon_msg(aniva_driver_t* this, dcc_t code, void* buffer, size_t size, void* out_buffer, size_t out_size);
int radeon_probe(pci_device_t* device, pci_driver_t* driver);

pci_driver_t radeon_pci = {
  .f_probe = radeon_probe,
  .id_table = radeon_id_table,
  .device_flags = NULL,
};

EXPORT_DRIVER(radeon_driver) = {
  .m_name = "radeon",
  .m_type = DT_GRAPHICS,
  .f_init = radeon_init,
  .f_exit = radeon_exit,
  .f_msg = radeon_msg,
  .m_precedence = DRV_PRECEDENCE_HIGH,
  .m_version = DEF_DRV_VERSION(1, 0, 0),
  .m_dep_count = 0,
  .m_dependencies = { 0 },
};

video_device_ops_t radeon_vdev_ops = {
  0
};

int radeon_init()
{
  /* Setup driver software stuff */

  /* Register pci driver */
  register_pci_driver(&radeon_pci);

  return 0;
}

int radeon_exit()
{
  /* Deinit driver software stuff =) */

  /* Remove pci driver */
  return unregister_pci_driver(&radeon_pci);
}

uintptr_t radeon_msg(aniva_driver_t* this, dcc_t code, void* buffer, size_t size, void* out_buffer, size_t out_size)
{
  return 0;
}

/*!
 * @brief Sets up the radeon hardware (for as far as we can :clown:)
 *
 * When the pci driver is registered, this prompts a new enumeration of the pci bus, which
 * will match the pci device attributes against the pci ids that we have specified support for. When
 * we find a matching ID, this function will be called and if successful, the enumeration will stop
 *
 * @returns: 0 on success, negative error code on failure (which we still need a good outline / framework
 * for lmao (TODO))
 */
int radeon_probe(pci_device_t* device, pci_driver_t* driver)
{
  logln("Found radeon yay");

  video_device_t* vdev = create_video_device(&radeon_driver, &radeon_vdev_ops);

  register_video_device(&radeon_driver, vdev);

  try_activate_video_device(vdev);

  kernel_panic("Found a radeon device =)");
  return 0;
}
