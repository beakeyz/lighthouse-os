#include "pci.h"
#include "bus.h"
#include "dev/debug/serial.h"
#include "dev/framebuffer/framebuffer.h"
#include "dev/pci/definitions.h"
#include "interupts/interupts.h"
#include "libk/error.h"
#include "libk/linkedlist.h"
#include "libk/stddef.h"
#include "libk/string.h"
#include <mem/heap.h>
#include "mem/kmem_manager.h"
#include "system/acpi/parser.h"
#include "system/acpi/structures.h"
#include "io.h"
#include <libk/io.h>

PciAccessMode_t s_current_addressing_mode;
list_t* g_pci_bridges;
list_t* g_pci_devices;
bool g_has_registered_bridges;
PciFuncCallback_t current_active_callback;
raw_pci_impls_t *early_raw_pci_impls;

/* Default PCI callbacks */

static ALWAYS_INLINE void pci_send_command(DeviceAddress_t* address, bool or_and, uint32_t shift);

void register_pci_devices(pci_device_identifier_t* dev) {
  list_t* list = g_pci_devices;
  pci_device_identifier_t* _dev = kmalloc(sizeof(pci_device_identifier_t));

  *_dev = *dev;

  list_append(list, _dev);
}

void print_device_info(pci_device_identifier_t* dev) {
  print(to_string(dev->vendor_id));
  putch(' ');
  println(to_string(dev->dev_id));
}

/* PCI enumeration stuff */

void enumerate_function(pci_bus_t* base_addr,uint8_t bus, uint8_t device, uint8_t func, PCI_FUNC_ENUMERATE_CALLBACK callback) {

  uint32_t index = base_addr->index;

  DeviceAddress_t address = {
    .index = index,
    .bus_num = bus,
    .device_num = device,
    .func_num = func
  };

  pci_device_identifier_t identifier = {
    .address = address
  };

  early_raw_pci_impls->read16(bus, device, func, DEVICE_ID, &identifier.dev_id);
  early_raw_pci_impls->read16(bus, device, func, VENDOR_ID, &identifier.vendor_id);
  early_raw_pci_impls->read16(bus, device, func, COMMAND, &identifier.command);
  early_raw_pci_impls->read16(bus, device, func, STATUS, &identifier.status);
  early_raw_pci_impls->read8(bus, device, func, HEADER_TYPE, &identifier.header_type);
  early_raw_pci_impls->read8(bus, device, func, CLASS, &identifier.class);
  early_raw_pci_impls->read8(bus, device, func, SUBCLASS, &identifier.subclass);
  early_raw_pci_impls->read8(bus, device, func, PROG_IF, &identifier.prog_if);
  early_raw_pci_impls->read8(bus, device, func, CACHE_LINE_SIZE, &identifier.cachelinesize);
  early_raw_pci_impls->read8(bus, device, func, LATENCY_TIMER, &identifier.latency_timer);
  early_raw_pci_impls->read8(bus, device, func, REVISION_ID, &identifier.revision_id);
  early_raw_pci_impls->read8(bus, device, func, INTERRUPT_LINE, &identifier.interrupt_line);
  early_raw_pci_impls->read8(bus, device, func, INTERRUPT_PIN, &identifier.interrupt_pin);
  early_raw_pci_impls->read8(bus, device, func, BIST, &identifier.BIST);

  if (identifier.dev_id == 0 || identifier.dev_id == PCI_NONE_VALUE) return;

  callback(&identifier);
}

void enumerate_device(pci_bus_t* base_addr, uint8_t bus, uint8_t device) {

  uint16_t cur_vendor_id;
  early_raw_pci_impls->read16(bus, device, 0, VENDOR_ID, &cur_vendor_id);

  if (cur_vendor_id == PCI_NONE_VALUE) {
    return;
  }

  const char* callback_name = current_active_callback.callback_name;

  enumerate_function(base_addr, bus, device, 0, callback_name != nullptr ? current_active_callback.callback : register_pci_devices);

  uint8_t cur_header_type;
  early_raw_pci_impls->read8( bus, device, 0, HEADER_TYPE, &cur_header_type);

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

void enumerate_bus(pci_bus_t* base_addr, uint8_t bus) {

  for (uintptr_t device = 0; device < 32; device++) {
    enumerate_device(base_addr, bus, device);
  }
}

void enumerate_bridges() {

  if (!g_has_registered_bridges) {
    return;
  }

  list_t* list = g_pci_bridges;

  FOREACH(i, list) {

    pci_bus_t* bridge = i->data;

    for (uintptr_t i = bridge->start_bus; i < bridge->end_bus; i++) {
      enumerate_bus(bridge, i);
    }
/*
    uint8_t should_enumerate_recusive = pci_io_field_read(0, 0, 0, HEADER_TYPE, PCI_8BIT_PORT_SIZE);

    if ((should_enumerate_recusive & 0x80) != 0) {
      // FIXME: might we be missing some of the bus? should look into this 
      for (uint8_t i = 1; i < 8; i++) {
        uint16_t vendor_id = pci_io_field_read(0, 0, i, VENDOR_ID, PCI_16BIT_PORT_SIZE);
        uint16_t class = pci_io_field_read(0, 0, i, CLASS, PCI_16BIT_PORT_SIZE);

        if (vendor_id == PCI_NONE_VALUE || class != BRIDGE_DEVICE) {
          continue;
        }

        enumerate_bus(bridge, bridge->start_bus + i);
      }
    }
    */
  }
}

void enumerate_pci_raw(PciFuncCallback_t callback) {
  set_current_enum_func(callback);
  enumerate_bridges();
}

void enumerate_registerd_devices(PCI_FUNC_ENUMERATE_CALLBACK callback) {

  FOREACH(i, g_pci_devices) {
    pci_device_identifier_t* dev = i->data;

    callback(dev);
  }
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

  for (uint32_t i = 0; i < entries; i++) {
    uint8_t start = mcfg->descriptors[i].start_bus;
    uint8_t end = mcfg->descriptors[i].end_bus;
    uint32_t base = mcfg->descriptors[i].base_addr;
  
    // add to pci bridge list
    pci_bus_t* bridge = kmalloc(sizeof(pci_bus_t));
    bridge->base_addr = base;
    bridge->start_bus = start;
    bridge->end_bus = end;
    bridge->index = i;
    bridge->is_mapped = false;
    bridge->mapped_base = nullptr;
    list_append(g_pci_bridges, bridge);
  }

  g_has_registered_bridges = true;
  return true;
}

// TODO:
bool test_pci_io_type2() {
  return false;
}

bool test_pci_io_type1() {

  uint32_t tmp;
  bool passed = false;

  out8(0xCFB, 0x01);

  tmp = in32(PCI_PORT_ADDR);

  out32(PCI_PORT_ADDR, 0x80000000);

  // if the value changes as expected, we have type1 I/O capabilities
  if (in32(PCI_PORT_ADDR) == 0x80000000) {
    passed = true;
  }

  // restore value
  out32(PCI_PORT_ADDR, tmp);

  return passed;
}

bool init_pci() {

  g_pci_devices = init_list();
  g_pci_bridges = init_list();

  MCFG_t* mcfg = find_table("MCFG");

  if (!mcfg) {
    kernel_panic("no mcfg found!");
//    return false;
  }

  bool success = register_pci_bridges_from_mcfg((uintptr_t)mcfg);

  if (test_pci_io_type1()) {
    s_current_addressing_mode = PCI_IOPORT_ACCESS;
    early_raw_pci_impls = (raw_pci_impls_t*)&raw_pci_type1_impl;
  } else if (test_pci_io_type2()) {
    kernel_panic("Detected PCI type 2 interfacing! currently not supported.");
  } else {
    kernel_panic("No viable pci access method found");
  }

  if (!success) {
    kernel_panic("Unable to register pci bridges from mcfg! (ACPI)");
  }

  PciFuncCallback_t callback = {
    .callback_name = "Register",
    .callback = register_pci_devices
  };

  enumerate_pci_raw(callback);

  // NOTE: test
  //enumerate_registerd_devices(print_device_info);

  return true;
}

pci_device_t create_pci_device(struct pci_bus* bus) {
  pci_device_t ret = {0};
  pci_device_impls_t impls;

  // TODO
  return ret;
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

pci_bus_t* get_bridge_by_index(uint32_t bridge_index) {
  FOREACH(i, g_pci_bridges) {
    pci_bus_t* bridge = i->data;
    if (bridge->index == bridge_index) {
      return bridge;
    }
  }
  return nullptr;
}

/*
 * TODO: these functions only seem to work in qemu, real hardware has some different method to get these 
 * fields memory mapped... let's figure out what the heck
 */
void pci_write_32(DeviceAddress_t* address, uint32_t field, uint32_t value) {
  pci_bus_t* bridge = get_bridge_by_index(address->index);
  if (bridge == nullptr) {
    return;
  }
  const uint8_t bus = address->bus_num;
  const uint8_t device = address->device_num;
  const uint8_t func = address->func_num;
  
  write_field32(bridge, bus, device, func, field, value);
}

void pci_write_16(DeviceAddress_t* address, uint32_t field, uint16_t value) {
  pci_bus_t* bridge = get_bridge_by_index(address->index);
  if (bridge == nullptr) {
    return;
  }
  const uint8_t bus = address->bus_num;
  const uint8_t device = address->device_num;
  const uint8_t func = address->func_num;
  
  write_field16(bridge, bus, device, func, field, value);
}

void pci_write_8(DeviceAddress_t* address, uint32_t field, uint8_t value) {
  pci_bus_t* bridge = get_bridge_by_index(address->index);
  if (bridge == nullptr) {
    return;
  }
  const uint8_t bus = address->bus_num;
  const uint8_t device = address->device_num;
  const uint8_t func = address->func_num;
  
  write_field8(bridge, bus, device, func, field, value);
}

uint32_t pci_read_32(DeviceAddress_t* address, uint32_t field) {
  pci_bus_t* bridge = get_bridge_by_index(address->index);
  if (bridge == nullptr) {
    return NULL;
  }
  const uint8_t bus = address->bus_num;
  const uint8_t device = address->device_num;
  const uint8_t func = address->func_num;
  
  return read_field32(bridge, bus, device, func, field);
}

uint16_t pci_read_16(DeviceAddress_t* address, uint32_t field) {
  pci_bus_t* bridge = get_bridge_by_index(address->index);
  if (bridge == nullptr) {
    return NULL;
  }
  uint8_t bus = address->bus_num;
  uint8_t device = address->device_num;
  uint8_t func = address->func_num;
  
  uint16_t result = read_field16(bridge, bus, device, func, field);
  return result;
}

uint8_t pci_read_8(DeviceAddress_t* address, uint32_t field) {
  pci_bus_t* bridge = get_bridge_by_index(address->index);
  if (bridge == nullptr) {
    return NULL;
  }
  const uint8_t bus = address->bus_num;
  const uint8_t device = address->device_num;
  const uint8_t func = address->func_num;
  
  return read_field8(bridge, bus, device, func, field);
}

/* PCI READ/WRITE WRAPPERS */
// TODO

static ALWAYS_INLINE void pci_send_command(DeviceAddress_t* address, bool or_and, uint32_t shift) {
  uint16_t placeback;
  early_raw_pci_impls->read16(address->bus_num, address->device_num, address->func_num, COMMAND, &placeback);

  if (or_and) {
    early_raw_pci_impls->write16(address->bus_num, address->device_num, address->func_num, COMMAND, placeback | (1 << shift));
  } else {
    early_raw_pci_impls->write16(address->bus_num, address->device_num, address->func_num, COMMAND, placeback & ~(1 << shift));
  }
}

void pci_set_io(DeviceAddress_t* address, bool value) {
  pci_send_command(address, value, 0);
}

void pci_set_memory(DeviceAddress_t* address, bool value) {
  pci_send_command(address, value, 1);
}

void pci_set_interrupt_line(DeviceAddress_t* address, bool value) {
  pci_send_command(address, !value, 10);
}

void pci_set_bus_mastering(DeviceAddress_t* address, bool value) {
  uint16_t placeback;
  early_raw_pci_impls->read16(address->bus_num, address->device_num, address->func_num, COMMAND, &placeback);

  if (value) {
    placeback |= (1 << 2);
  } else {
    placeback &= ~(1 << 2);
  }
  placeback |= (1 << 0);

  early_raw_pci_impls->write16(address->bus_num, address->device_num, address->func_num, COMMAND, placeback);
}

PciAccessMode_t get_current_addressing_mode() {
  return s_current_addressing_mode;
}
