#ifndef __ANIVA_AHCI_CONTROLLER__
#define __ANIVA_AHCI_CONTROLLER__
#include "dev/disk/ahci/definitions.h"
#include "dev/driver.h"
#include "libk/data/linkedlist.h"
#include <dev/pci/pci.h>

struct ahci_port;
struct drv_manifest;
struct dgroup;

typedef struct ahci_device {
    pci_device_t* m_identifier;

    volatile HBA* m_hba_region;
    list_t* m_ports;

    uint32_t m_idx;
    uint32_t m_used_ports;
    uint32_t m_available_ports;

    struct drv_manifest* m_parent;
    struct ahci_device* m_next;
} ahci_device_t;

ahci_device_t* create_ahci_device(pci_device_t* identifier);
void destroy_ahci_device(ahci_device_t* device);

uint32_t ahci_mmio_read32(uintptr_t base, uintptr_t offset);
void ahci_mmio_write32(uintptr_t base, uintptr_t offset, uint32_t data);

#endif // !__ANIVA_AHCI_CONTROLLER__
