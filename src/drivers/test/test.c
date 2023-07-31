#include "dev/debug/serial.h"
#include <dev/core.h>
#include <dev/driver.h>
#include <libk/stddef.h>
#include <dev/pci/pci.h>
#include <dev/pci/definitions.h>

pci_dev_id_t test_ids[] = {
  PCI_DEVID_CLASSES_EX(NETWORK_CONTROLLER, 0, 0, PCI_DEVID_USE_CLASS),
  PCI_DEVID_END,
};

int test_probe(pci_device_t* dev, pci_driver_t* driver)
{
  println("Found a potential device!");

  return 0;
}

pci_driver_t test_pci_driver = {
  .id_table = test_ids,
  .f_probe = test_probe,
};

int test_init() {
  println("Initalizing test driver!");

  register_pci_driver(&test_pci_driver);

  return 0;
}

EXPORT_DRIVER(extern_test_driver) 
  =
{
  .m_name = "ExternalTest",
  .m_descriptor = "Just funnie test",
  .m_version = DRIVER_VERSION(0, 0, 1),
  .m_type = DT_OTHER,
  .f_init = test_init,
  .m_dep_count = 0,
};
