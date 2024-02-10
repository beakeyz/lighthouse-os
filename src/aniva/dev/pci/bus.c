#include "bus.h"
#include "dev/device.h"
#include "dev/group.h"
#include "libk/flow/error.h"
#include "libk/stddef.h"
#include "mem/heap.h"
#include "pci.h"
#include "io.h"
#include <mem/kmem_manager.h>
#include <libk/string.h>

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
  const char* prefix = "bus";
  const size_t index_len = strlen(to_string(busnum));
  const size_t total_len = strlen(prefix) + index_len + 1;
  char* ret = kmalloc(total_len);

  memset(ret, 0, total_len);
  memcpy(ret, prefix, strlen(prefix));
  memcpy(ret + strlen(prefix), to_string(busnum), index_len);

  return ret;
}

pci_bus_t* create_pci_bus(uint32_t base, uint8_t start, uint8_t end, uint32_t busnum, pci_bus_t* parent)
{
  pci_bus_t* bus;
  dgroup_t* parent_group;
  const char* busname;

  bus = kmalloc(sizeof(*bus));

  if (!bus)
    return nullptr;

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

  bus->dev = create_device_ex(NULL, (char*)busname, bus, NULL, NULL, NULL);
  bus->dev->bus_group = register_dev_group(DGROUP_TYPE_PCI, to_string(busnum), NULL, parent_group->node);

  return bus;
}

void* map_bus(pci_bus_t* this, uint8_t bus_num) {
  uintptr_t bus_base_addr = this->base_addr + (MEM_SIZE_PER_BUS * (bus_num - this->start_bus));

  if (!this->is_mapped || this->mapped_base == nullptr) {
    void* mapped_bus_base = (void*)Must(__kmem_kernel_alloc(bus_base_addr, MEM_SIZE_PER_BUS, 0, 0));

    this->mapped_base = mapped_bus_base;
    this->is_mapped = true;
  }

  return this->mapped_base;
}

void write_field32(pci_bus_t* this, uint8_t bus, uint8_t device, uint8_t function, uint32_t field, uint32_t value) {
  ASSERT(field <= 0xffc);
  void* _base = map_bus(this, bus) + (SMALL_PAGE_SIZE * function + (SMALL_PAGE_SIZE * MAX_FUNC_PER_DEV) * device);
  void* base = _base + (field & 0xfff);
  memcpy(base, (void*)(uintptr_t)value, sizeof(uint32_t));
}

void write_field16(pci_bus_t* this, uint8_t bus, uint8_t device, uint8_t function, uint32_t field, uint16_t value) {
  ASSERT(field <= 0xfff);
  void* _base = map_bus(this, bus) + (SMALL_PAGE_SIZE * function + (SMALL_PAGE_SIZE * MAX_FUNC_PER_DEV) * device);
  void* base = _base + (field & 0xfff);
  memcpy(base, (void*)(uintptr_t)value, sizeof(uint16_t));
}

void write_field8(pci_bus_t* this, uint8_t bus, uint8_t device, uint8_t function, uint32_t field, uint8_t value) {
  ASSERT(field <= 0xfff);
  void* _base = map_bus(this, bus) + (SMALL_PAGE_SIZE * function + (SMALL_PAGE_SIZE * MAX_FUNC_PER_DEV) * device);
  void* base = _base + (field & 0xfff);
  *((uint8_t volatile*)base) = value;
}

uint32_t read_field32(pci_bus_t* this, uint8_t bus, uint8_t device, uint8_t function, uint32_t field) {
  uint32_t ret = 0;

  ASSERT(field <= 0xffc);
  void* _base = map_bus(this, bus) + (SMALL_PAGE_SIZE * function + (SMALL_PAGE_SIZE * MAX_FUNC_PER_DEV) * device);
  void* base = _base + (field & 0xfff);

  memcpy(&ret, base, sizeof(uint32_t));

  return ret;
}

uint16_t read_field16(pci_bus_t* this, uint8_t bus, uint8_t device, uint8_t function, uint32_t field) {
  uint16_t ret = 0;

  ASSERT(field <= 0xffc);
  void* _base = map_bus(this, bus) + (SMALL_PAGE_SIZE * function + (SMALL_PAGE_SIZE * MAX_FUNC_PER_DEV) * device);
  void* base = _base + (field & 0xfff);

  memcpy(&ret, base, sizeof(uint16_t));

  return ret;
}

uint8_t read_field8(pci_bus_t* this, uint8_t bus, uint8_t device, uint8_t function, uint32_t field) {

  ASSERT(field <= 0xffc);

  switch (get_current_addressing_mode()) {
    case PCI_IOPORT_ACCESS: {
      uint8_t ret;
      g_pci_type1_impl.read8(bus, device, function, field, &ret);
      return (volatile uint8_t)ret;
    }
    case PCI_MEM_ACCESS: {
      void *_base = map_bus(this, bus) + (SMALL_PAGE_SIZE * function + (SMALL_PAGE_SIZE * MAX_FUNC_PER_DEV) * device);
      void *base = _base + (field & 0xfff);
      return *((uint8_t volatile *) base);
    }
    case PCI_UNKNOWN_ACCESS:
      return 0;
  }
  return 0;
}

void init_pci_bus()
{
  _pci_group = register_dev_group(DGROUP_TYPE_PCI, "pci", NULL, NULL); 
}
