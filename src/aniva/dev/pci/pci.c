#include "pci.h"
#include "bus.h"
#include "dev/debug/serial.h"
#include "dev/pci/definitions.h"
#include "interrupts/interrupts.h"
#include "libk/flow/error.h"
#include "libk/data/linkedlist.h"
#include "libk/stddef.h"
#include "libk/string.h"
#include <mem/heap.h>
#include "mem/kmem_manager.h"
#include "mem/zalloc.h"
#include "sync/mutex.h"
#include "system/acpi/parser.h"
#include "system/acpi/structures.h"
#include "io.h"
#include "system/resource.h"
#include <libk/io.h>

static pci_accessmode_t __current_addressing_mode;
static pci_device_ops_io_t* __current_pci_access_impl;
static list_t* __pci_bridges;
static list_t* __pci_devices;

static zone_allocator_t* __pci_dev_allocator;

bool __has_registered_bridges;

struct pci_driver_link {
  pci_driver_t* this;
  struct pci_driver_link* next;
};

static struct pci_driver_link* __pci_drivers;

#define FOREACH_PCI_DRIVER(i, drivers) for (struct pci_driver_link* i = drivers; i; i = i->next)

static struct pci_driver_link** __find_pci_driver(pci_driver_t* driver)
{
  struct pci_driver_link** ret = &__pci_drivers;

  for (; *ret; ret = &(*ret)->next) {
    if ((*ret)->this == driver)
      return ret;
  }

  return ret;
}

static bool __matches_pci_devid(pci_device_t* device, pci_dev_id_t id) 
{
  /* Ids */
  if (PCI_DEVID_SHOULD(id.usage_bits, PCI_DEVID_USE_VENDOR_ID) && device->vendor_id != id.ids.vendor_id)
    return false;

  if (PCI_DEVID_SHOULD(id.usage_bits, PCI_DEVID_USE_DEVICE_ID) && device->dev_id != id.ids.device_id)
    return false;

  /* Classes */
  if (PCI_DEVID_SHOULD(id.usage_bits, PCI_DEVID_USE_CLASS) && device->class != id.classes.class)
    return false;

  if (PCI_DEVID_SHOULD(id.usage_bits, PCI_DEVID_USE_SUBCLASS) && device->subclass != id.classes.subclass)
    return false;

  if (PCI_DEVID_SHOULD(id.usage_bits, PCI_DEVID_USE_PROG_IF) && device->prog_if != id.classes.prog_if)
    return false;

  return true;
}

/*
 * Try to see if there is a suitable device present for this driver to manage
 * NOTE: the caller may not have the drivers lock held on call
 * FIXME: when the device already has a driver, we should try to determine whether to
 *        replace the old driver with this one...
 *
 * @returns: the amount of fitting devices found
 */
static int __find_fitting_pci_devices(pci_driver_t* driver)
{
  pci_device_t* ret;
  pci_dev_id_t* ids;

  /* If we can't probe, thats kinda cringe tbh */
  if (!driver->f_probe)
    return -1;

  /* TODO: remove this assert */
  ASSERT(!driver->device_count);

  int probe_error;
  ids = driver->id_table;
  driver->device_count = 0;

  mutex_lock(driver->lock);

  FOREACH(i, __pci_devices) {
    ret = i->data;

    /* For now, skip any device that already has a driver, until we can choose one over the other */
    if (ret->driver)
      continue;

    FOREACH_PCI_DEVID(i, ids) {
      if (__matches_pci_devid(ret, *i)) {
        /* Try to probe */
        probe_error = driver->f_probe(ret, driver);

        if (!probe_error) {
          driver->device_count++;
          ret->driver = driver;
        }
      }

      probe_error = 0;
    }
  }

  mutex_unlock(driver->lock);
  return driver->device_count;
}

/*!
 * @brief Try to add the pci driver to the link
 *
 * Fails if it either alread exists, or if we could not find a fitting device
 * Nothing to add here...
 */
static int __add_pci_driver(pci_driver_t* driver) 
{
  struct pci_driver_link** slot;

  slot = __find_pci_driver(driver);

  /* Does it exists? */
  if (*slot)
    return -1;

  /* This takes and releases the drivers lock */
  if (!__find_fitting_pci_devices(driver))
    return -1;

  /* We found some devices for this driver, AND a slot in the link! happy day */
  *slot = kzalloc(sizeof(struct pci_driver_link));

  (*slot)->this = driver;
  (*slot)->next = nullptr;

  return 0;
}

static int __remove_pci_driver(pci_driver_t* driver)
{
  struct pci_driver_link** slot;
  struct pci_driver_link* to_remove;

  slot = __find_pci_driver(driver);

  if (*slot) {
    to_remove = *slot;

    *slot = to_remove->next;
    
    kzfree(*slot, sizeof(struct pci_driver_link));

    return 0;
  }

  return -1;
}

static bool __has_pci_driver(pci_driver_t* driver)
{
  struct pci_driver_link** link;

  link = __find_pci_driver(driver);

  if (!(*link))
    return false;

  if (!(*link)->this)
    return false;

  return true;
}


/*
 * Register a pci device to our local bus and try to find a pci driver for it
 */
static void __register_pci_device(pci_device_t* dev) {

  int probe_error;
  pci_driver_t* fitting_driver;
  pci_device_t* _dev;

  if (!dev || !__pci_devices || !__pci_dev_allocator)
    return;

  fitting_driver = nullptr;

  /*
   * TODO: we might have multiple drivers that cover the same devices.
   * we should give every device a value that describes how sophisticated they 
   * are, with which we can determine if we want to load this driver, or look for 
   * one that is more sophisticated and can thus be used far more
   */

  /* Loop over every registered pci driver */
  FOREACH_PCI_DRIVER(i, __pci_drivers) {
    pci_driver_t* d = i->this;

    /* Loop over their supported ids */
    FOREACH_PCI_DEVID(dev_id, d->id_table) {

      /* Try to match */
      if (__matches_pci_devid(dev, *dev_id)) {
        probe_error = d->f_probe(dev, d);
        fitting_driver = d;
        break;
      }
    }

    /* Have we found our driver? */
    if (fitting_driver) 
      break;
  }

  /* Yay, this pci device can be piloted by a fitting driver! */
  if (fitting_driver && !probe_error) {
    dev->driver = fitting_driver;
  }

  /* Allocate the device */
  _dev = zalloc_fixed(__pci_dev_allocator);

  memcpy(_dev, dev, sizeof(pci_device_t));

  list_append(__pci_devices, _dev);
}

/* Default PCI callbacks */

static ALWAYS_INLINE void pci_send_command(pci_device_address_t* address, bool or_and, uint32_t shift);

bool is_end_devid(pci_dev_id_t* dev_id)
{
  pci_dev_id_t end_dev = PCI_DEVID_END;
  return (memcmp(dev_id, &end_dev, sizeof(pci_dev_id_t)));
}

bool is_pci_driver_unused(pci_driver_t* driver)
{
  if (!__has_pci_driver(driver))
    return false;

  FOREACH(i, __pci_devices) {
    pci_device_t* device = i->data;

    if (device->driver == driver)
      return true;
  }

  return false;
}

/*
 * TODO: registering a new pci device should prompt rescan of the bus
 * in order to figure out where we should put this driver
 *
 * NOTE: This function may fail when we can't find a device for this driver to govern
 */
int register_pci_driver(struct pci_driver* driver)
{
  if (!driver->id_table)
    return -1;

  driver->lock = create_mutex(NULL);

  return __add_pci_driver(driver);
}

int unregister_pci_driver(struct pci_driver* driver)
{
  destroy_mutex(driver->lock);

  return __remove_pci_driver(driver);
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

  pci_device_address_t* address;

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

  pci_device_address_t* address;

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
    .raw_ops = *__current_pci_access_impl,
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
  __current_pci_access_impl->read16(bus, device, 0, VENDOR_ID, &cur_vendor_id);

  if (cur_vendor_id == PCI_NONE_VALUE) {
    return;
  }

  enumerate_function(callback, base_addr, bus, device, 0);

  uint8_t cur_header_type;
  __current_pci_access_impl->read8( bus, device, 0, HEADER_TYPE, &cur_header_type);

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

/*
 * TODO: verify syntax
 */
void pci_device_register_resource(pci_device_t* device, uint32_t index)
{
  //uintptr_t value;
  //resource_claim_ex("PCI_resource", uintptr_t start, size_t size, kresource_type_t type, kresource_t **regions)
  (void)device;
  (void)index;
  kernel_panic("Impl");
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

/*
 * Enumerate over all the pci devices present on the bus and try to find 
 * drivers for them. This should be called after we have loaded/installed all our 
 * drivers.
 */
void init_pci_drivers()
{
  pci_callback_t callback = {
    .callback_name = "Register",
    .callback = __register_pci_device
  };

  enumerate_bridges(&callback);
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
    __current_pci_access_impl = &g_pci_type1_impl;
  } else if (test_pci_io_type2()) {
    kernel_panic("Detected PCI type 2 interfacing! currently not supported.");
    __current_pci_access_impl = &g_pci_type2_impl;
  } else {
    kernel_panic("No viable pci access method found");
  }

  if (!success) {
    kernel_panic("Unable to register pci bridges from mcfg! (ACPI)");
  }
  return true;
}

pci_device_t create_pci_device(struct pci_bus* bus) {
  kernel_panic("Impl: create_pci_device");
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
void pci_write_32(pci_device_address_t* address, uint32_t field, uint32_t value) {
  pci_bus_t* bridge = get_bridge_by_index(address->index);
  if (bridge == nullptr) {
    return;
  }
  const uint8_t bus = address->bus_num;
  const uint8_t device = address->device_num;
  const uint8_t func = address->func_num;
  
  write_field32(bridge, bus, device, func, field, value);
}

void pci_write_16(pci_device_address_t* address, uint32_t field, uint16_t value) {
  pci_bus_t* bridge = get_bridge_by_index(address->index);
  if (bridge == nullptr) {
    return;
  }
  const uint8_t bus = address->bus_num;
  const uint8_t device = address->device_num;
  const uint8_t func = address->func_num;
  
  write_field16(bridge, bus, device, func, field, value);
}

void pci_write_8(pci_device_address_t* address, uint32_t field, uint8_t value) {
  pci_bus_t* bridge = get_bridge_by_index(address->index);
  if (bridge == nullptr) {
    return;
  }
  const uint8_t bus = address->bus_num;
  const uint8_t device = address->device_num;
  const uint8_t func = address->func_num;
  
  write_field8(bridge, bus, device, func, field, value);
}

uint32_t pci_read_32(pci_device_address_t* address, uint32_t field) {
  pci_bus_t* bridge = get_bridge_by_index(address->index);
  if (bridge == nullptr) {
    return NULL;
  }
  const uint8_t bus = address->bus_num;
  const uint8_t device = address->device_num;
  const uint8_t func = address->func_num;
  
  return read_field32(bridge, bus, device, func, field);
}

uint16_t pci_read_16(pci_device_address_t* address, uint32_t field) {
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

uint8_t pci_read_8(pci_device_address_t* address, uint32_t field) {
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

static ALWAYS_INLINE void pci_send_command(pci_device_address_t* address, bool or_and, uint32_t shift) {
  uint16_t placeback;
  __current_pci_access_impl->read16(address->bus_num, address->device_num, address->func_num, COMMAND, &placeback);

  if (or_and) {
    __current_pci_access_impl->write16(address->bus_num, address->device_num, address->func_num, COMMAND, placeback | shift);
  } else {
    __current_pci_access_impl->write16(address->bus_num, address->device_num, address->func_num, COMMAND, placeback & ~(shift));
  }
}

void pci_set_io(pci_device_address_t* address, bool value) {
  pci_send_command(address, value, PCI_COMMAND_IO_SPACE);
}

void pci_set_memory(pci_device_address_t* address, bool value) {
  pci_send_command(address, value, PCI_COMMAND_MEM_SPACE);
}

void pci_set_interrupt_line(pci_device_address_t* address, bool value) {
  pci_send_command(address, !value, PCI_COMMAND_INT_DISABLE);
}

void pci_set_bus_mastering(pci_device_address_t* address, bool value) {
  pci_send_command(address, value, PCI_COMMAND_BUS_MASTER);
}

pci_accessmode_t get_current_addressing_mode() {
  return __current_addressing_mode;
}
