#ifndef __ANIVA_AHCI_CONTROLLER__
#define __ANIVA_AHCI_CONTROLLER__
#include "dev/disk/ahci/definitions.h"
#include <dev/pci/pci.h>

struct ahci_port;

typedef struct ahci_device {
  pci_device_identifier_t* m_identifier;

  void* m_hba_region;
  list_t* m_ports;

  uint32_t m_used_ports;
  // TODO
} ahci_device_t;

ahci_device_t* init_ahci_device(pci_device_identifier_t* identifier);

#endif // !__ANIVA_AHCI_CONTROLLER__