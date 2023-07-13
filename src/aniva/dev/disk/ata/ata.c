#include "ata.h"
#include "dev/core.h"
#include "dev/debug/serial.h"
#include "dev/pci/pci.h"
#include <dev/pci/definitions.h>

static void find_ata_controller(pci_device_t* device) {
  if (device->address.device_num == MASS_STORAGE) {
    if (device->address.bus_num == PCI_SUBCLASS_ATA) {
      println("ATA: found an ata controller");
    } else if (device->address.bus_num == PCI_SUBCLASS_IDE) {
      println("ATA: found an ide controller");
    }
  }
}

int ata_driver_init() {

  println("Initializing generic ATA driver");

  enumerate_registerd_devices(find_ata_controller);

  return 0;
}

int ata_driver_exit() {
  return 0;
}

uintptr_t ata_driver_on_packet(packet_payload_t payload, packet_response_t** response) {
  return 0;
}

const aniva_driver_t g_base_ata_driver = {
  .m_name = "ata",
  .m_type = DT_DISK,
  .m_ident = DRIVER_IDENT(0, 0),
  .m_version = DRIVER_VERSION(0, 0, 1),
  .f_init = ata_driver_init,
  .f_exit = ata_driver_exit,
  .f_drv_msg = ata_driver_on_packet,
  .m_port = 6,
  .m_dependencies = {},
  .m_dep_count = 0,
};
EXPORT_DRIVER(g_base_ata_driver);

