#include "dev/device.h"
#include "dev/manifest.h"
#include <dev/core.h>
#include <dev/driver.h>
#include <libk/stddef.h>
#include <dev/pci/pci.h>
#include <dev/pci/definitions.h>

#define DRIVER_NAME "debug"
#define DRIVER_DESCRIPTOR "Perform various driver-related debuging"

#define TEST_DEVICE "testdev"

device_t* device;
drv_manifest_t* manifest;

int test_init(drv_manifest_t* driver);
int test_exit();

EXPORT_DEPENDENCIES(deps) = {
  DRV_DEP_END,
};

EXPORT_DRIVER(extern_test_driver) = {
  .m_name = DRIVER_NAME,
  .m_descriptor = DRIVER_DESCRIPTOR,
  .m_version = DRIVER_VERSION(0, 0, 1),
  .m_type = DT_OTHER,
  .f_init = test_init,
  .f_exit = test_exit,
};

struct device_generic_endpoint generic = {
  NULL,
};

device_ep_t eps[] = {
  { ENDPOINT_TYPE_GENERIC, sizeof(generic), { &generic, } },
  { NULL },
};

/*
 * This driver will be used to test kernel subsystems targeting drivers. Here is a list of things that
 * have been tested:
 *  - PCI device scanning
 * And here is a list of things that are currently being tested:
 *  - Driver-bound devices
 */
int test_init(drv_manifest_t* driver) 
{
  device = create_device_ex(driver, TEST_DEVICE, NULL, NULL, eps);

  /* Register to the devicegroup root */
  device_register(device, NULL);

  logln("Initalizing test driver!");

  return 0;
}

/*!
 * @brief: Perform driver cleanup
 *
 * Make sure we remove the device from this driver on unload
 * NOTE: a device should never be persistant through driver lifetime, so 
 * any devices without manifests should be considered fatal errors
 */
int test_exit() 
{
  device_unregister(device);
  destroy_device(device);
  logln("Exiting test driver! =D");
  return 0;
}
