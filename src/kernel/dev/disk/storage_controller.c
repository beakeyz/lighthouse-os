#include "storage_controller.h"
#include "dev/debug/serial.h"
#include "dev/disk/ahci/AhciController.h"
#include "dev/pci/definitions.h"
#include "dev/pci/pci.h"
#include "libk/linkedlist.h"

list_t* g_controllers = nullptr;

static void find_storage_device(DeviceIdentifier_t* identifier);

static void find_storage_device(DeviceIdentifier_t* identifier) {
  // 
  if (identifier->class == MASSSTORAGE) {

    MassstorageSubClasIDType_t type = identifier->subclass;
    
    // 0x01 == AHCI progIF
    if (type == SATA_C && identifier->prog_if == 0x01) {
      // ahci controller
      list_append(g_controllers, init_ahci_device(identifier));
    }
  }
}


void init_storage_controller() {

  println("Initialising storage driver");

  g_controllers = init_list();

  enumerate_registerd_devices(find_storage_device);

}
