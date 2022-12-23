#include "pci.h"
#include "dev/debug/serial.h"
#include "libk/linkedlist.h"
#include "libk/stddef.h"
#include "libk/string.h"
#include "mem/kmalloc.h"
#include "mem/kmem_manager.h"
#include "system/acpi/structures.h"
#include <libk/io.h>

list_t g_pci_bridges;
bool g_has_registered_bridges;

void print_device_info(DeviceIdentifier_t* dev) {
  print(to_string(dev->vendor_id));
  putch(' ');
  println(to_string(dev->dev_id));
}

void enumerate_function(uint64_t base_addr, uint64_t func, PCI_FUNC_ENUMERATE_CALLBACK callback) {

  uint64_t offset = func << 12;

  uintptr_t fun_addr = base_addr + offset;

  // identitymap the funnie
  kmem_map_page(nullptr, fun_addr, fun_addr, KMEM_CUSTOMFLAG_GET_MAKE);

  DeviceIdentifier_t* identifier = (DeviceIdentifier_t*)fun_addr;

  if (identifier->dev_id == 0 || identifier->dev_id == 0xFFFF) return;

  callback(identifier);

}

void enumerate_devices(uint64_t base_addr, uint64_t device) {
  uint64_t offset = device << 15;

  uintptr_t dev_addr = base_addr + offset;

  // identitymap the funnie
  kmem_map_page(nullptr, dev_addr, dev_addr, KMEM_CUSTOMFLAG_GET_MAKE);

  DeviceIdentifier_t* identifier = (DeviceIdentifier_t*)dev_addr;

  if (identifier->dev_id == 0 || identifier->dev_id == 0xFFFF) return;

  for (uintptr_t func = 0; func < 32; func++) {
    enumerate_function(dev_addr, func, print_device_info);
  }
}

void enumerate_bus(uint64_t base_addr, uint64_t bus) {
  uint64_t offset = bus << 20;

  uintptr_t bus_addr = base_addr + offset;

  // identitymap the funnie
  kmem_map_page(nullptr, bus_addr, bus_addr, KMEM_CUSTOMFLAG_GET_MAKE);

  DeviceIdentifier_t* identifier = (DeviceIdentifier_t*)bus_addr;

  if (identifier->dev_id == 0 || identifier->dev_id == 0xFFFF) return;

  for (uintptr_t device = 0; device < 32; device++) {
    enumerate_devices(bus_addr, device);
  }
}

void enumerate_bridges(PCI_ENUMERATE_CALLBACK callback) {

  if (!g_has_registered_bridges) {
    return;
  }

  list_t* list = &g_pci_bridges;
  ITTERATE(list);
  
  PCI_Bridge_t* bridge = itterator->data;

  for (uint64_t i = bridge->start_bus; i < bridge->end_bus; i++) {
    callback(bridge->base_addr, i);
  }

  ENDITTERATE(list);
}

bool register_pci_bridges_from_mcfg(uintptr_t mcfg_ptr) {

  SDT_header_t* header = kmem_kernel_alloc(mcfg_ptr, sizeof(SDT_header_t), KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE);

  uint32_t length = header->length;
  
  if (length == sizeof(SDT_header_t)) {
    return false;
  }

  length = ALIGN_UP(length + SMALL_PAGE_SIZE, SMALL_PAGE_SIZE);

  MCFG_t* mcfg = (MCFG_t*)kmem_kernel_alloc(mcfg_ptr, length, KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE);
  uint32_t entries = (mcfg->header.length - sizeof(MCFG_t)) / sizeof(PCI_Device_Descriptor_t);

  for (int i = 0; i < entries; i++) {
    uint8_t start = mcfg->descriptors[i].start_bus;
    uint8_t end = mcfg->descriptors[i].end_bus;
    uint32_t base = mcfg->descriptors[i].base_addr;
  
    // add to pci bridge list
    PCI_Bridge_t* bridge = kmalloc(sizeof(PCI_Bridge_t));
    bridge->base_addr = base;
    bridge->start_bus = start;
    bridge->end_bus = end;
    bridge->index = i;
    list_append(&g_pci_bridges, bridge);
    println("append");
  }

  g_has_registered_bridges = true;
  return true;
}

uint32_t pci_field_read (uint32_t device_num, uint32_t field, uint32_t size) {
  
  // setup
  out32(PCI_PORT_ADDR, GET_PCI_ADDR(device_num, field));

  switch (size) {
    case 4: {
      uint32_t ret = in32(PCI_PORT_VALUE);
      return ret;
    }
    case 2: {
      uint16_t ret = in16(PCI_PORT_VALUE + (field & 2));
      return ret;
    }
    case 1: {
      uint8_t ret = in8(PCI_PORT_VALUE + (field & 3));
      return ret;
    }
    default:
      return 0xFFFF; 
  }
}

void pci_field_write (uint32_t device_num, uint32_t field, uint32_t size, uint32_t val) {
  out32(PCI_PORT_ADDR, GET_PCI_ADDR(device_num, field));
  out32(PCI_PORT_VALUE, val);
}

bool test_pci_io () {

  uint32_t tmp = 0x80000000;
  out32(PCI_PORT_ADDR, tmp);
  tmp = in32(PCI_PORT_ADDR);
  // if the value remains unchanged, we have I/O capabilities is PCI
  if (tmp == 0x80000000) {
    return true;
  }
  return false;
}

