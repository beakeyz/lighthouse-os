#include "bus.h"
#include "dev/device.h"
#include "dev/group.h"
#include "libk/stddef.h"
#include "logging/log.h"
#include "mem/heap.h"
#include "pci.h"
#include <libk/string.h>
#include <mem/kmem_manager.h>
#include <oss/node.h>

static dgroup_t* _pci_group;

/*
 * FIXME (And TODO): This shit is broken af
 * We need to get right how this PCI bus is being provided to us by the firmware (ACPI)
 * This means the bus might only be accessable through the IO configuration space, but there
 * might also be an I/O memory mapped ECAM region. Acpi should know about it, so we should
 * probably ask them
 */

#define MAX_FUNC_PER_DEV 8
#define MAX_DEV_PER_BUS 32
#define MEM_SIZE_PER_BUS (SMALL_PAGE_SIZE * MAX_FUNC_PER_DEV * MAX_DEV_PER_BUS)

static const char* _create_bus_name(uint32_t busnum)
{
    char* buffer = kmalloc(16);

    memset(buffer, 0, 16);

    if (sfmt(buffer, "bus%d", busnum)) {
        kfree(buffer);
        return nullptr;
    }

    return buffer;
}

pci_bus_t* create_pci_bus(uint32_t base, uint8_t start, uint8_t end, uint32_t busnum, pci_bus_t* parent)
{
    pci_bus_t* bus;
    dgroup_t* parent_group;
    const char* busname;

    bus = kmalloc(sizeof(*bus));

    if (!bus)
        return nullptr;

    memset(bus, 0, sizeof(*bus));

    if (!parent)
        parent_group = _pci_group;
    else
        parent_group = parent->dev->bus_group;

    bus->base_addr = base;
    bus->mapped_base = nullptr;
    bus->is_mapped = false;

    bus->index = busnum;
    bus->start_bus = start;
    bus->end_bus = end;

    busname = _create_bus_name(busnum);

    /* Create the device for this bus */
    bus->dev = create_device_ex(NULL, (char*)busname, bus, NULL, NULL);
    bus->dev->bus_group = register_dev_group(DGROUP_TYPE_PCI, to_string(busnum), NULL, parent_group->node);

    dev_group_add_device(parent_group, bus->dev);

    return bus;
}

void init_pci_bus()
{
    _pci_group = register_dev_group(DGROUP_TYPE_PCI, "pci", NULL, NULL);
}
