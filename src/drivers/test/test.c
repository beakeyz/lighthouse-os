#include "dev/device.h"
#include "dev/manifest.h"
#include "libk/flow/error.h"
#include "libk/string.h"
#include "mem/kmem_manager.h"
#include <dev/core.h>
#include <dev/driver.h>
#include <libk/stddef.h>
#include <dev/pci/pci.h>
#include <dev/pci/definitions.h>

#define DRIVER_NAME "ExternalTest"

dev_manifest_t* manifest;
device_t* our_device;

int test_init();
int test_exit();

EXPORT_DRIVER(extern_test_driver) = {
  .m_name = DRIVER_NAME,
  .m_descriptor = "Just funnie test",
  .m_version = DRIVER_VERSION(0, 0, 1),
  .m_type = DT_OTHER,
  .f_init = test_init,
  .f_exit = test_exit,
  .m_dep_count = 0,
};


/*
 * This driver will be used to test kernel subsystems targeting drivers. Here is a list of things that
 * have been tested:
 *  - PCI device scanning
 * And here is a list of things that are currently being tested:
 *  - Driver-bound devices
 */
int test_init() 
{
  logln("Initalizing test driver!");

  manifest = try_driver_get(&extern_test_driver, NULL);

  ASSERT(manifest);

  /* This device should be accessable through :/Dev/other/ExternalTest/testdev */
  our_device = create_device("testdev");

  manifest_add_device(manifest, our_device);

  return 0;
}

int test_exit() 
{
  logln("Exiting test driver! =D");
  return 0;
}
