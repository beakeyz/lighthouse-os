#include "AhciController.h"
#include "dev/debug/serial.h"
#include "dev/pci/pci.h"
#include "libk/string.h"
#include <mem/kmalloc.h>

// TODO:
AhciDevice_t* init_ahci_device(DeviceIdentifier_t* identifier) {
  AhciDevice_t* ret = kmalloc(sizeof(AhciDevice_t));

  ret->m_identifier = identifier;

  return ret;
}
