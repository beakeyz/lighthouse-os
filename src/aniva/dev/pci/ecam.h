#ifndef __ANIVA_PCIE_ECAM__
#define __ANIVA_PCIE_ECAM__

#include "bus.h"

#define PCIE_ECAM_BUS_SHIFT 20 /* Bus number */
#define PCIE_ECAM_DEVFN_SHIFT 12 /* Device and Function number */

#define PCIE_ECAM_BUS_MASK 0xff
#define PCIE_ECAM_DEVFN_MASK 0xff
#define PCIE_ECAM_REG_MASK 0xfff /* Limit offset to a maximum of 4K */

#define PCIE_ECAM_BUS(x) (((x) & PCIE_ECAM_BUS_MASK) << PCIE_ECAM_BUS_SHIFT)
#define PCIE_ECAM_DEVFN(x) (((x) & PCIE_ECAM_DEVFN_MASK) << PCIE_ECAM_DEVFN_SHIFT)
#define PCIE_ECAM_REG(x) ((x) & PCIE_ECAM_REG_MASK)

#define PCIE_ECAM_OFFSET(bus, devfn, where) \
    (PCIE_ECAM_BUS(bus) | PCIE_ECAM_DEVFN(devfn) | PCIE_ECAM_REG(where))

typedef struct pci_ecam_config_window {

    void* m_window;
} pci_ecam_config_window_t;

void pci_map_ecam_bus(pci_bus_t* bus, uint32_t device_function, int where);

#endif //__ANIVA_PCIE_ECAM__
