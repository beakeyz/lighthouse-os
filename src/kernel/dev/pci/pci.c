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

void enumerate_function(PCI_Bridge_t* base_addr,uint8_t bus, uint8_t device, uint8_t func, PCI_FUNC_ENUMERATE_CALLBACK callback) {

  DeviceIdentifier_t identifier = {
    .dev_id = read_field16(base_addr, bus, device, func, DEVICE_ID),
    .vendor_id = read_field16(base_addr, bus, device, func, VENDOR_ID),
    .header_type = read_field8(base_addr, bus, device, func, HEADER_TYPE),
    .class = read_field8(base_addr, bus, device, func, CLASS),
    .subclass = read_field8(base_addr, bus, device, func, SUBCLASS),
    .prog_if = read_field8(base_addr, bus, device, func, PROG_IF),
  };

  if (identifier.dev_id == 0 || identifier.dev_id == PCI_NONE_VALUE) return;

  callback(&identifier);

}

void enumerate_devices(PCI_Bridge_t* base_addr, uint8_t bus, uint8_t device) {

  uint16_t cur_vendor_id = read_field16(base_addr, bus, device, 0, VENDOR_ID);

  if (cur_vendor_id == PCI_NONE_VALUE) {
    return;
  }

  const char* callback_name = current_active_callback.callback_name;

  enumerate_function(base_addr, bus, device, 0, callback_name != nullptr ? current_active_callback.callback : print_device_info);

  uint8_t cur_header_type = read_field8(base_addr, bus, device, 0, HEADER_TYPE);

  if (!(cur_header_type & 0x80)) {
    return;
  }

  for (uint8_t func = 1; func < 8; func++) {
    if (current_active_callback.callback_name != nullptr) {
      enumerate_function(base_addr, bus, device, func, current_active_callback.callback);
    } else {
      enumerate_function(base_addr, bus, device, func, print_device_info);
    }
  }
}

void enumerate_bus(PCI_Bridge_t* base_addr, uint8_t bus) {

  for (uintptr_t device = 0; device < 32; device++) {
    enumerate_devices(base_addr, bus, device);
  }
}

void enumerate_bridges() {

  if (!g_has_registered_bridges) {
    return;
  }

  list_t* list = &g_pci_bridges;
  ITTERATE(list);
  
  PCI_Bridge_t* bridge = itterator->data;

  // FIXME: we are now enumerating the ENTIRE possible bus range, overkill?
  for (uint8_t i = bridge->start_bus; i < bridge->end_bus; i++) {
    enumerate_bus(bridge, i);
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

