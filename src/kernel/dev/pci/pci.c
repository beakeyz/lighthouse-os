#include "pci.h"
#include "bridge.h"
#include "dev/debug/serial.h"
#include "libk/error.h"
#include "libk/linkedlist.h"
#include "libk/stddef.h"
#include "libk/string.h"
#include "mem/kmalloc.h"
#include "mem/kmem_manager.h"
#include "system/acpi/structures.h"
#include <libk/io.h>

list_t g_pci_bridges;
list_t g_pci_devices;
bool g_has_registered_bridges;
PciFuncCallback_t current_active_callback;

void register_pci_devices(DeviceIdentifier_t* dev) {
  list_t* list = &g_pci_devices;
  node_t* next_node = kmalloc(sizeof(node_t));

  next_node->data = dev;

  list_append(list, next_node);
  println("append device");
}

void print_device_info(DeviceIdentifier_t* dev) {
  print(to_string(dev->vendor_id));
  putch(' ');
  println(to_string(dev->dev_id));
}

void enumerate_function(uint64_t base_addr,uint8_t bus, uint8_t device, uint8_t func, PCI_FUNC_ENUMERATE_CALLBACK callback) {

  uint64_t offset = func << 12;

  uintptr_t fun_addr = base_addr + offset;

  // identitymap the funnie
  kmem_map_page(nullptr, fun_addr, fun_addr, KMEM_CUSTOMFLAG_GET_MAKE);

  DeviceIdentifier_t* identifier = (DeviceIdentifier_t*)fun_addr;

  if (identifier->dev_id == 0 || identifier->dev_id == 0xFFFF) return;

  uint16_t vendor_id = read_field16(g_pci_bridges.head->data, bus, device, func, VENDOR_ID);

  println(to_string(vendor_id));

  callback(identifier);

}

void enumerate_devices(uint64_t base_addr, uint8_t bus, uint8_t device) {
  uint64_t offset = device << 15;

  uintptr_t dev_addr = base_addr + offset;

  // identitymap the funnie
  kmem_map_page(nullptr, dev_addr, dev_addr, KMEM_CUSTOMFLAG_GET_MAKE);

  DeviceIdentifier_t* identifier = (DeviceIdentifier_t*)dev_addr;

  if (identifier->dev_id == 0 || identifier->dev_id == 0xFFFF) return;

  for (uintptr_t func = 0; func < 8; func++) {
    if (current_active_callback.callback_name != nullptr) {
      enumerate_function(dev_addr, bus, device, func, current_active_callback.callback);
    } else {
      enumerate_function(dev_addr, bus, device, func, print_device_info);
    }
  }
}

void enumerate_bus(uint64_t base_addr, uint8_t bus) {
  uint64_t offset = bus << 20;

  uintptr_t bus_addr = base_addr + offset;

  // identitymap the funnie
  kmem_map_page(nullptr, bus_addr, bus_addr, KMEM_CUSTOMFLAG_GET_MAKE);

  DeviceIdentifier_t* identifier = (DeviceIdentifier_t*)bus_addr;

  if (identifier->dev_id == 0 || identifier->dev_id == 0xFFFF) return;

  for (uintptr_t device = 0; device < 32; device++) {
    enumerate_devices(bus_addr, bus, device);
  }
}

void enumerate_bridges() {

  if (!g_has_registered_bridges) {
    return;
  }

  list_t* list = &g_pci_bridges;
  ITTERATE(list);
  
  PCI_Bridge_t* bridge = itterator->data;

  for (uint8_t i = bridge->start_bus; i < bridge->end_bus; i++) {
    enumerate_bus(bridge->base_addr, i);
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
    case PCI_32BIT_PORT_SIZE: {
      uint32_t ret = in32(PCI_PORT_VALUE);
      return ret;
    }
    case PCI_16BIT_PORT_SIZE: {
      uint16_t ret = in16(PCI_PORT_VALUE + (field & 2));
      return ret;
    }
    case PCI_8BIT_PORT_SIZE: {
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

bool init_pci() {

  if (!g_has_registered_bridges) {
    kernel_panic("Trying to init pci before bridges have been registered!");
  }

  bool has_pci_io = test_pci_io();

  if (has_pci_io) {
    println("Pci io capabilities confirmed!");
  }

  set_pci_func(register_pci_devices, "Register devices");

  enumerate_bridges();

  set_pci_func(print_device_info, "Print device ids");

  enumerate_bridges();

  return false;
}

bool set_pci_func(PCI_FUNC_ENUMERATE_CALLBACK callback, const char* name) {

  if (name == nullptr || !callback) {
    return false;
  }

  PciFuncCallback_t pfc = {
    .callback_name = name,
    .callback = callback
  };

  set_current_enum_func(pfc);

  return true;
}

bool set_current_enum_func(PciFuncCallback_t new_callback) {
  // gotta name it something
  if (new_callback.callback_name != nullptr) {
    current_active_callback = new_callback;
    return true;
  }

  return false;
}

