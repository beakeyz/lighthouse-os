#include "ahci.h"
#include "dev/ahci/AhciController.h"
#include "dev/debug/serial.h"
#include "dev/pci/definitions.h"
#include "libk/linkedlist.h"
#include "libk/stddef.h"
#include <dev/pci/pci.h>

list_t* g_controllers = nullptr;

static void find_ahci_device(DeviceIdentifier_t* identifier);

static void find_ahci_device(DeviceIdentifier_t* identifier) {
  if (identifier->class == MASSSTORAGE) {

    MassstorageSubClasIDType_t type = identifier->subclass;
    
    // 0x01 == AHCI progIF
    if (type == SATA_C && identifier->prog_if == 0x01) {

      list_append(g_controllers, init_ahcidevice(identifier));
    }
  }
}

void init_ahci() {

  g_controllers = init_list();

  enumerate_registerd_devices(find_ahci_device);

  AhciDevice_t* test = list_get(g_controllers, 0);
  if (test != nullptr) {
    print_device_info(test->m_identifier);
  }
}
