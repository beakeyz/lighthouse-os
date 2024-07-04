#include "pci.h"

/*!
 * @brief: Allocate an IRQ for a specific PCI device
 *
 * We may use 4 methods to handle PCI irqs:
 *  - MSI
 *  - MSI-X
 *  - I/O Apic
 *  - Regular dual PIC
 *
 * TODO
 */
int pci_device_allocate_irq(pci_device_t* device, uint32_t irq_flags, uint32_t handler_flags, void* handler, void* ctx, const char* desc)
{

    return 0;
}
