#include "pci.h"
#include "bridge.h"
#include "dev/debug/serial.h"
#include "libk/error.h"
#include "libk/linkedlist.h"
#include "libk/stddef.h"
#include "libk/string.h"
#include "mem/kmalloc.h"
#include "mem/kmem_manager.h"
#include "system/acpi/parser.h"
#include "system/acpi/structures.h"
#include <libk/io.h>

list_t* g_pci_bridges;
list_t* g_pci_devices;
bool g_has_registered_bridges;
PciFuncCallback_t current_active_callback;

/* Default PCI callbacks */

static ALWAYS_INLINE void pci_send_command(DeviceAddress_t* address, bool or_and, uint32_t shift);

void register_pci_devices(DeviceIdentifier_t* dev) {
  list_t* list = g_pci_devices;
  DeviceIdentifier_t* _dev = kmalloc(sizeof(DeviceIdentifier_t));

  memcpy(_dev, dev, sizeof(DeviceIdentifier_t));

  list_append(list, _dev);
}

void print_device_info(DeviceIdentifier_t* dev) {
  print(to_string(dev->vendor_id));
  putch(' ');
  println(to_string(dev->dev_id));
}

/* PCI enumeration stuff */

void enumerate_function(PCI_Bridge_t* base_addr,uint8_t bus, uint8_t device, uint8_t func, PCI_FUNC_ENUMERATE_CALLBACK callback) {

  uint32_t index = base_addr->index;

  DeviceAddress_t address = {
    .index = index,
    .bus_num = bus,
    .device_num = device,
    .func_num = func
  };

  DeviceIdentifier_t identifier = {
    .dev_id = read_field16(base_addr, bus, device, func, DEVICE_ID),
    .vendor_id = read_field16(base_addr, bus, device, func, VENDOR_ID),
    .command = read_field16(base_addr, bus, device, func, COMMAND),
    .status = read_field16(base_addr, bus, device, func, STATUS),
    .header_type = read_field8(base_addr, bus, device, func, HEADER_TYPE),
    .class = read_field8(base_addr, bus, device, func, CLASS),
    .subclass = read_field8(base_addr, bus, device, func, SUBCLASS),
    .prog_if = read_field8(base_addr, bus, device, func, PROG_IF),
    .cachelinesize = read_field8(base_addr, bus, device, func, CACHE_LINE_SIZE),
    .latency_timer = read_field8(base_addr, bus, device, func, LATENCY_TIMER),
    .revision_id = read_field8(base_addr, bus, device, func, REVISION_ID),
    .interrupt_line = read_field8(base_addr, bus, device, func, INTERRUPT_LINE),
    .interrupt_pin = read_field8(base_addr, bus, device, func, INTERRUPT_PIN),
    .BIST = read_field8(base_addr, bus, device, func, BIST),
    .address = address
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

  list_t* list = g_pci_bridges;

  FOREACH(i, list) {

    PCI_Bridge_t* bridge = i->data;

    enumerate_bus(bridge, bridge->start_bus);

    uint8_t should_enumerate_recusive = read_field8(bridge, 0, 0, 0, HEADER_TYPE);

    if ((should_enumerate_recusive & 0x80) != 0) {
      // FIXME: might we be missing some of the bus? should look into this 
      for (uint8_t i = 1; i < 8; i++) {
        uint16_t vendor_id = read_field16(bridge, 0, 0, i, VENDOR_ID);
        uint16_t class = read_field16(bridge, 0, 0, i, CLASS);

        if (vendor_id == PCI_NONE_VALUE || class != 0x6) {
          continue;
        }

        enumerate_bus(bridge, bridge->start_bus + i);
      }
    }
  }
}

void enumerate_pci_raw(PciFuncCallback_t callback) {
  set_current_enum_func(callback);
  enumerate_bridges();
}

void enumerate_registerd_devices(PCI_FUNC_ENUMERATE_CALLBACK callback) {

  FOREACH(i, g_pci_devices) {
    DeviceIdentifier_t* dev = i->data;

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
    PCI_Bridge_t* bridge = kmalloc(sizeof(PCI_Bridge_t));
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

  g_pci_devices = init_list();
  g_pci_bridges = init_list();

  MCFG_t* mcfg = find_table("MCFG");

  bool success = register_pci_bridges_from_mcfg((uintptr_t)mcfg);
  bool has_pci_io = test_pci_io();

  if (success) {
    if (!g_has_registered_bridges) {
      kernel_panic("Trying to init pci before bridges have been registered!");
    }

    if (has_pci_io) {
      println("Pci io capabilities confirmed!");
    }

    PciFuncCallback_t callback = {
      .callback_name = "Register",
      .callback = register_pci_devices
    };

    enumerate_pci_raw(callback);

    // NOTE: test
    enumerate_registerd_devices(print_device_info);

    return true;
  }

  // TODO: try to initialize PCI with IO addressing

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

PCI_Bridge_t* get_bridge_by_index(uint32_t bridge_index) {
  FOREACH(i, g_pci_bridges) {
    PCI_Bridge_t* bridge = i->data;
    if (bridge->index == bridge_index) {
      return bridge;
    }
  }
  return nullptr;
}

void pci_write_32(DeviceAddress_t* address, uint32_t field, uint32_t value) {
  PCI_Bridge_t* bridge = get_bridge_by_index(address->index);
  if (bridge == nullptr) {
    return;
  }
  const uint8_t bus = address->bus_num;
  const uint8_t device = address->device_num;
  const uint8_t func = address->func_num;
  
  write_field32(bridge, bus, device, func, field, value);
}

void pci_write_16(DeviceAddress_t* address, uint32_t field, uint16_t value) {
  PCI_Bridge_t* bridge = get_bridge_by_index(address->index);
  if (bridge == nullptr) {
    return;
  }
  const uint8_t bus = address->bus_num;
  const uint8_t device = address->device_num;
  const uint8_t func = address->func_num;
  
  write_field16(bridge, bus, device, func, field, value);
}

void pci_write_8(DeviceAddress_t* address, uint32_t field, uint8_t value) {
  PCI_Bridge_t* bridge = get_bridge_by_index(address->index);
  if (bridge == nullptr) {
    return;
  }
  const uint8_t bus = address->bus_num;
  const uint8_t device = address->device_num;
  const uint8_t func = address->func_num;
  
  write_field8(bridge, bus, device, func, field, value);
}

uint32_t pci_read_32(DeviceAddress_t* address, uint32_t field) {
  PCI_Bridge_t* bridge = get_bridge_by_index(address->index);
  if (bridge == nullptr) {
    return NULL;
  }
  const uint8_t bus = address->bus_num;
  const uint8_t device = address->device_num;
  const uint8_t func = address->func_num;
  
  return read_field32(bridge, bus, device, func, field);
}

uint16_t pci_read_16(DeviceAddress_t* address, uint32_t field) {
  PCI_Bridge_t* bridge = get_bridge_by_index(address->index);
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
  PCI_Bridge_t* bridge = get_bridge_by_index(address->index);
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
  if (or_and) {
    pci_write_16(address, COMMAND, pci_read_16(address, COMMAND) | (1 << shift));
  } else {
    pci_write_16(address, COMMAND, pci_read_16(address, COMMAND) & ~(1 << shift));
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
  uint16_t placeback = pci_read_16(address, COMMAND);

  if (value) {
    placeback |= (1 << 2);
  } else {
    placeback &= ~(1 << 2);
  }
  placeback |= (1 << 0);

  pci_write_16(address, COMMAND, placeback);
}
