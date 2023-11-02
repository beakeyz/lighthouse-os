#ifndef __ANIVA_NVD_PCI_DEVICE__
#define __ANIVA_NVD_PCI_DEVICE__

#include "dev/pci/pci.h"
#include <libk/stddef.h>

typedef struct nv_pci_device_id {
  uint16_t device;
  uint16_t vendor;
  const char* label;
} nv_pci_device_id_t;

typedef struct nv_device {
  nv_pci_device_id_t* id;
  pci_device_t* pdevice;
} nv_device_t;

nv_device_t* create_nv_device(pci_device_t* pdev);

#endif // !__ANIVA_NVD_PCI_DEVICE__
