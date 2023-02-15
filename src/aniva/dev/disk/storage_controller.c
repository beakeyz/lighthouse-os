#include "storage_controller.h"
#include "dev/debug/serial.h"
#include "dev/disk/ahci/ahci_device.h"
#include "dev/framebuffer/framebuffer.h"
#include "dev/pci/definitions.h"
#include "dev/pci/pci.h"
#include "libk/linkedlist.h"
#include <libk/string.h>

list_t* g_controllers = nullptr;

static void find_storage_device(pci_device_identifier_t* identifier);

static void find_storage_device(pci_device_identifier_t* identifier) {

  if (identifier->class == MASS_STORAGE) {

    static uintptr_t idx = 0;

    const char* cls = (char*)to_string(identifier->class);

    for (int i = 0; i < strlen(cls); i++) {
      draw_char(idx, 32, cls[i]);
      idx+= 8;
    }
    idx += 16;

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
