#include "bus.h"
#include "dev/debug/serial.h"
#include "libk/error.h"
#include "libk/stddef.h"
#include "pci.h"
#include "io.h"
#include <mem/kmem_manager.h>
#include <libk/string.h>

#define MAX_FUNC_PER_DEV 8
#define MAX_DEV_PER_BUS 32

#define MEM_SIZE_PER_BUS (SMALL_PAGE_SIZE * MAX_FUNC_PER_DEV * MAX_DEV_PER_BUS)

void* map_bus(pci_bus_t* this, uint8_t bus_num) {
  uintptr_t bus_base_addr = this->base_addr + (MEM_SIZE_PER_BUS * (bus_num - this->start_bus));

  if (!this->is_mapped || this->mapped_base == nullptr) {
    void* mapped_bus_base = kmem_kernel_alloc(bus_base_addr, MEM_SIZE_PER_BUS, 0);

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
      early_raw_pci_impls->read8(bus, device, function, field, &ret);
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
