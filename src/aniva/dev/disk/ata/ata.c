#include "ata.h"
#include "dev/core.h"
#include "dev/debug/serial.h"
#include "dev/pci/pci.h"
#include <dev/pci/definitions.h>

static pci_dev_id_t ata_dev_ids[] = {
  PCI_DEVID_CLASSES_EX(MASS_STORAGE, PCI_SUBCLASS_ATA, 0, PCI_DEVID_USE_CLASS | PCI_DEVID_USE_SUBCLASS),
  PCI_DEVID_CLASSES_EX(MASS_STORAGE, PCI_SUBCLASS_IDE, 0, PCI_DEVID_USE_CLASS | PCI_DEVID_USE_SUBCLASS),
  PCI_DEVID_END,
};

uintptr_t ata_driver_on_packet(packet_payload_t payload, packet_response_t** response) {
  return 0;
}

int ata_probe(pci_device_t* device, pci_driver_t* driver)
{
  println("Found ATA device!");
  return 0;
}

static pci_driver_t ata_pci_driver = {
  .id_table = ata_dev_ids,
  .f_probe = ata_probe,
};

int ata_driver_init() {

  println("Initializing generic ATA driver");

  register_pci_driver(&ata_pci_driver);
  //enumerate_registerd_devices(find_ata_controller);

  return 0;
}

int ata_driver_exit() {
  return 0;
}


const aniva_driver_t g_base_ata_driver = {
  .m_name = "ata",
  .m_type = DT_DISK,
  .m_version = DRIVER_VERSION(0, 0, 1),
  .f_init = ata_driver_init,
  .f_exit = ata_driver_exit,
  .f_drv_msg = ata_driver_on_packet,
  .m_port = 6,
  .m_dependencies = {},
  .m_dep_count = 0,
};
EXPORT_DRIVER(g_base_ata_driver);

