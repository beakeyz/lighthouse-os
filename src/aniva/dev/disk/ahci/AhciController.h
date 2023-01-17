#ifndef __ANIVA_AHCI_CONTROLLER__
#define __ANIVA_AHCI_CONTROLLER__
#include "dev/disk/ahci/definitions.h"
#include <dev/pci/pci.h>

struct AhciPort;

typedef struct AhciDevice {
  DeviceIdentifier_t* m_identifier;

  void* m_hba_region;
  list_t* m_ports;

  uint32_t m_used_ports;
  // TODO
} AhciDevice_t;

AhciDevice_t* init_ahci_device(DeviceIdentifier_t* identifier);

#endif // !__ANIVA_AHCI_CONTROLLER__
