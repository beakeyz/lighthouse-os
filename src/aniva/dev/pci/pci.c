#include "pci.h"
#include "bus.h"
#include "dev/debug/serial.h"
#include "dev/framebuffer/framebuffer.h"
#include "dev/pci/definitions.h"
#include "interrupts/interrupts.h"
#include "libk/flow/error.h"
#include "libk/data/linkedlist.h"
#include "libk/stddef.h"
#include "libk/string.h"
#include <mem/heap.h>
#include "mem/kmem_manager.h"
#include "mem/zalloc.h"
#include "system/acpi/parser.h"
#include "system/acpi/structures.h"
#include "io.h"
#include <libk/io.h>

pci_accessmode_t __current_addressing_mode;
list_t* __pci_bridges;
list_t* __pci_devices;
zone_allocator_t* __pci_dev_allocator;
bool __has_registered_bridges;

/* Default PCI callbacks */

static ALWAYS_INLINE void pci_send_command(DeviceAddress_t* address, bool or_and, uint32_t shift);

void register_pci_devices(pci_device_t* dev) {
  pci_device_t* _dev;

  if (!dev || !__pci_devices || !__pci_dev_allocator)
    return;

  _dev = zalloc_fixed(__pci_dev_allocator);

  memcpy(_dev, dev, sizeof(pci_device_t));

  list_append(__pci_devices, _dev);
}

void print_device_info(pci_device_t* dev) {
  print(to_string(dev->vendor_id));
  putch(' ');
  println(to_string(dev->dev_id));
}

uintptr_t get_pci_register_size(pci_register_offset_t offset)
{
  switch (offset) {
    case BAR0:
    case BAR1:
    case BAR2:
    case BAR3:
    case BAR4:
    case BAR5:
    case ROM_ADDRESS:
      return 32;
    case VENDOR_ID:
    case DEVICE_ID:
    case COMMAND:
    case STATUS:
    case SUBSYSTEM_VENDOR_ID:
    case SUBSYSTEM_ID:
      return 16;
    default:
      return 8;
  }
}

bool pci_register_is_std(pci_register_offset_t offset)
{
  return (offset >= FIRST_REGISTER && offset <= LAST_REGISTER);
}

static int pci_dev_write(pci_device_t* device, uint32_t field, uintptr_t __value)
{

  DeviceAddress_t* address;

  if (!device)
    return -1;

  address = &device->address;

  /* TODO: support non-std registers with this */
  if (!pci_register_is_std(field))
    return -1;

  switch (get_pci_register_size(field)) {
    case 32:
      {
        uint32_t value = (uint32_t)__value;
        return device->raw_ops.write32(address->bus_num, address->device_num, address->func_num, field, value);
      }
    case 16:
      {
        uint16_t value = (uint16_t)__value;
        return device->raw_ops.write16(address->bus_num, address->device_num, address->func_num, field, value);
      }
    case 8:
      {
        uint8_t value = (uint8_t)__value;
        return device->raw_ops.write8(address->bus_num, address->device_num, address->func_num, field, value);
      }
  }
  return -1;
}

static int pci_dev_read(pci_device_t* device, uint32_t field, uintptr_t* __value)
{

  DeviceAddress_t* address;

  if (!device)
    return -1;

  address = &device->address;

  /* TODO: support non-std registers with this */
  if (!pci_register_is_std(field))
    return -1;

  switch (get_pci_register_size(field)) {
    case 32:
      {
        uint32_t* value = (uint32_t*)__value;
        return device->raw_ops.read32(address->bus_num, address->device_num, address->func_num, field, value);
      }
    case 16:
      {
        uint16_t* value = (uint16_t*)__value;
        return device->raw_ops.read16(address->bus_num, address->device_num, address->func_num, field, value);
      }
    case 8:
      {
        uint8_t* value = (uint8_t*)__value;
        return device->raw_ops.read8(address->bus_num, address->device_num, address->func_num, field, value);
      }
  }
  return -1;
}

/* PCI enumeration stuff */

void enumerate_function(pci_callback_t* callback, pci_bus_t* base_addr,uint8_t bus, uint8_t device, uint8_t func) {

  uint32_t index;

  if (!callback)
    return;
  
  index = base_addr->index;

  pci_device_t identifier = {
    .address = {
      .index = index,
      .bus_num = bus,
      .device_num = device,
      .func_num = func
    },
    .raw_ops = g_pci_type1_impl,
    .ops = {
      .read = pci_dev_read,
      .write = pci_dev_write,
    }
  };

  identifier.raw_ops.read16(bus, device, func, DEVICE_ID, &identifier.dev_id);
  identifier.raw_ops.read16(bus, device, func, VENDOR_ID, &identifier.vendor_id);
  identifier.raw_ops.read16(bus, device, func, COMMAND, &identifier.command);
  identifier.raw_ops.read16(bus, device, func, STATUS, &identifier.status);
  identifier.raw_ops.read8(bus, device, func, HEADER_TYPE, &identifier.header_type);
  identifier.raw_ops.read8(bus, device, func, CLASS, &identifier.class);
  identifier.raw_ops.read8(bus, device, func, SUBCLASS, &identifier.subclass);
  identifier.raw_ops.read8(bus, device, func, PROG_IF, &identifier.prog_if);
  identifier.raw_ops.read8(bus, device, func, CACHE_LINE_SIZE, &identifier.cachelinesize);
  identifier.raw_ops.read8(bus, device, func, LATENCY_TIMER, &identifier.latency_timer);
  identifier.raw_ops.read8(bus, device, func, REVISION_ID, &identifier.revision_id);
  identifier.raw_ops.read8(bus, device, func, INTERRUPT_LINE, &identifier.interrupt_line);
  identifier.raw_ops.read8(bus, device, func, INTERRUPT_PIN, &identifier.interrupt_pin);
  identifier.raw_ops.read8(bus, device, func, BIST, &identifier.BIST);

  if (identifier.dev_id == 0 || identifier.dev_id == PCI_NONE_VALUE) return;

  callback->callback(&identifier);
}

void enumerate_device(pci_callback_t* callback, pci_bus_t* base_addr, uint8_t bus, uint8_t device) {

  uint16_t cur_vendor_id;
  g_pci_type1_impl.read16(bus, device, 0, VENDOR_ID, &cur_vendor_id);

  if (cur_vendor_id == PCI_NONE_VALUE) {
    return;
  }

  enumerate_function(callback, base_addr, bus, device, 0);

  uint8_t cur_header_type;
  g_pci_type1_impl.read8( bus, device, 0, HEADER_TYPE, &cur_header_type);

  if (!(cur_header_type & 0x80)) {
    return;
  }

  for (uint8_t func = 1; func < 8; func++) {
    enumerate_function(callback, base_addr, bus, device, func);
  }
}

void enumerate_bus(pci_callback_t* callback, pci_bus_t* base_addr, uint8_t bus) {

  for (uintptr_t device = 0; device < 32; device++) {
    enumerate_device(callback, base_addr, bus, device);
  }
}

void enumerate_bridges(pci_callback_t* callback) {

  if (!__has_registered_bridges) {
    return;
  }

  list_t* list = __pci_bridges;

  FOREACH(i, list) {

    pci_bus_t* bridge = i->data;

    for (uintptr_t i = bridge->start_bus; i < bridge->end_bus; i++) {
      enumerate_bus(callback, bridge, i);
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

void enumerate_registerd_devices(PCI_FUNC_ENUMERATE_CALLBACK callback) {

  if (__pci_devices == nullptr) {
    return;
  }

  FOREACH(i, __pci_devices) {
    pci_device_t* dev = i->data;

    callback(dev);
  }
}

bool register_pci_bridges_from_mcfg(acpi_mcfg_t* mcfg_ptr) {

  acpi_sdt_header_t* header = &mcfg_ptr->header;

  uint32_t length = header->length;
  
  if (length == sizeof(acpi_sdt_header_t)) {
    return false;
  }

  length = ALIGN_UP(length + SMALL_PAGE_SIZE, SMALL_PAGE_SIZE);

  uint32_t entries = (header->length - sizeof(acpi_mcfg_t)) / sizeof(PCI_Device_Descriptor_t);

  for (uint32_t i = 0; i < entries; i++) {
    uint8_t start = mcfg_ptr->descriptors[i].start_bus;
    uint8_t end = mcfg_ptr->descriptors[i].end_bus;
    uint32_t base = mcfg_ptr->descriptors[i].base_addr;
  
    // add to pci bridge list
    pci_bus_t* bridge = kmalloc(sizeof(pci_bus_t));
    bridge->base_addr = base;
    bridge->start_bus = start;
    bridge->end_bus = end;
    bridge->index = i;
    bridge->is_mapped = false;
    bridge->mapped_base = nullptr;
    list_append(__pci_bridges, bridge);
  }

  __has_registered_bridges = true;
  return true;
}

uintptr_t get_bar_address(uint32_t bar)
{
  if (is_bar_io(bar)) {
    return (uintptr_t)(bar & PCI_BASE_ADDRESS_IO_MASK);
  }

  ASSERT_MSG(is_bar_mem(bar), "Invalid bar?");

  return (uintptr_t)(bar & PCI_BASE_ADDRESS_MEM_MASK);
}

bool is_bar_io(uint32_t bar)
{
  return (bar & PCI_BASE_ADDRESS_SPACE) == PCI_BASE_ADDRESS_SPACE_IO;
}

bool is_bar_mem(uint32_t bar)
{
  return (bar & PCI_BASE_ADDRESS_SPACE) == PCI_BASE_ADDRESS_SPACE_MEM;
}

uint8_t bar_get_type(uint32_t bar)
{
  return (uint8_t)(bar & PCI_BASE_ADDRESS_MEM_TYPE_MASK);
}

bool is_bar_16bit(uint32_t bar)
{
  return (!is_bar_32bit(bar) && !is_bar_16bit(bar));
}

bool is_bar_32bit(uint32_t bar)
{
  return (bar_get_type(bar) == PCI_BASE_ADDRESS_MEM_TYPE_32);
}

bool is_bar_64bit(uint32_t bar)
{
  return (bar_get_type(bar) == PCI_BASE_ADDRESS_MEM_TYPE_64);
}

void pci_device_register_resource(pci_device_t* device, uint32_t index)
{
  uintptr_t value;

}

void pci_device_unregister_resource(pci_device_t* device, uint32_t index)
{
  kernel_panic("Impl");
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

  __pci_devices = init_list();
  __pci_bridges = init_list();

  __pci_dev_allocator = create_zone_allocator(28 * Kib, sizeof(pci_device_t), NULL);

  acpi_mcfg_t* mcfg = find_table(g_parser_ptr, "MCFG");

  if (!mcfg) {
    kernel_panic("no mcfg found!");
  }

  bool success = register_pci_bridges_from_mcfg(mcfg);

  if (test_pci_io_type1()) {
    __current_addressing_mode = PCI_IOPORT_ACCESS;
  } else if (test_pci_io_type2()) {
    kernel_panic("Detected PCI type 2 interfacing! currently not supported.");
  } else {
    kernel_panic("No viable pci access method found");
  }

  if (!success) {
    kernel_panic("Unable to register pci bridges from mcfg! (ACPI)");
  }

  pci_callback_t callback = {
    .callback_name = "Register",
    .callback = register_pci_devices
  };

  enumerate_bridges(&callback);

  return true;
}

pci_device_t create_pci_device(struct pci_bus* bus) {
  pci_device_t ret = {0};
  pci_device_ops_io_t impls;

  // TODO
  return ret;
}

pci_bus_t* get_bridge_by_index(uint32_t bridge_index) {
  FOREACH(i, __pci_bridges) {
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
  g_pci_type1_impl.read16(address->bus_num, address->device_num, address->func_num, COMMAND, &placeback);

  if (or_and) {
    g_pci_type1_impl.write16(address->bus_num, address->device_num, address->func_num, COMMAND, placeback | shift);
  } else {
    g_pci_type1_impl.write16(address->bus_num, address->device_num, address->func_num, COMMAND, placeback & ~(shift));
  }
}

void pci_set_io(DeviceAddress_t* address, bool value) {
  pci_send_command(address, value, PCI_COMMAND_IO_SPACE);
}

void pci_set_memory(DeviceAddress_t* address, bool value) {
  pci_send_command(address, value, PCI_COMMAND_MEM_SPACE);
}

void pci_set_interrupt_line(DeviceAddress_t* address, bool value) {
  pci_send_command(address, !value, PCI_COMMAND_INT_DISABLE);
}

void pci_set_bus_mastering(DeviceAddress_t* address, bool value) {
  pci_send_command(address, value, PCI_COMMAND_BUS_MASTER);
}

pci_accessmode_t get_current_addressing_mode() {
  return __current_addressing_mode;
}
