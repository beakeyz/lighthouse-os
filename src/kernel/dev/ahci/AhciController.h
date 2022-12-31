#ifndef __LIGHT_AHCI_CONTROLLER__
#define __LIGHT_AHCI_CONTROLLER__
#include <dev/pci/pci.h>

typedef struct {
  DeviceIdentifier_t* m_identifier;

} AhciDevice_t;

AhciDevice_t* init_ahcidevice(DeviceIdentifier_t* identifier);

#endif // !__LIGHT_AHCI_CONTROLLER__
