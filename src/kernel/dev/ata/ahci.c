#include "ahci.h"
#include "dev/pci/definitions.h"
#include <dev/pci/pci.h>

static void find_ahci_device(DeviceIdentifier_t* identifier);

static void find_ahci_device(DeviceIdentifier_t* identifier) {
  if (identifier->class == MASSSTORAGE) {

  }
}

void init_ahci() {

  enumerate_registerd_devices(find_ahci_device);
}
