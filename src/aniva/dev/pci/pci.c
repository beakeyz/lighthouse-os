#include "pci.h"
#include "bus.h"
#include "dev/group.h"
#include "dev/pci/definitions.h"
#include "libk/flow/error.h"
#include "libk/data/linkedlist.h"
#include "libk/stddef.h"
#include <mem/heap.h>
#include "mem/kmem_manager.h"
#include "mem/zalloc.h"
#include "sync/mutex.h"
#include "system/acpi/acpi.h"
#include "system/acpi/parser.h"
#include "io.h"
#include <libk/io.h>

static pci_accessmode_t __current_addressing_mode = PCI_UNKNOWN_ACCESS;
static pci_device_ops_io_t* __current_pci_access_impl = NULL;
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
  int probe_error;
  pci_device_t* ret;
  pci_dev_id_t* ids;

  /* If we can't probe, thats kinda cringe tbh */
  if (!driver->f_probe)
    return 0;

  /* TODO: remove this assert */
  ASSERT(!driver->device_count);

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

  /* Link the driver before we find a fitting device */
  *slot = kzalloc(sizeof(struct pci_driver_link));

  (*slot)->this = driver;
  (*slot)->next = nullptr;

  /* A driver may just know of itself that it should not prompt a rescan here */
  if ((driver->device_flags & PCI_DEV_FLAG_NO_RESCAN) == PCI_DEV_FLAG_NO_RESCAN)
    return 0;

  /*
   * This takes and releases the drivers lock 
   * NOTE: we ignore the 
   */
  if (!__find_fitting_pci_devices(driver) &&
      (driver->device_flags & PCI_DEV_FLAG_VOLATILE_RESCAN) == PCI_DEV_FLAG_VOLATILE_RESCAN)
    return -1;

  /*
   * If we could find a device, great! otherwise __remove_pci_driver is most likely to be 
   * called in a bit, so that's why we have the driver linked before finding a driver
   */
  return 0;
}

static int __remove_pci_driver(pci_driver_t* driver)
{
  struct pci_driver_link** slot;
  struct pci_driver_link* to_remove;

  slot = __find_pci_driver(driver);

  if (*slot) {
    to_remove = *slot;

    to_remove->this = nullptr;

    *slot = to_remove->next;
    
    kzfree(to_remove, sizeof(struct pci_driver_link));

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

  if (dev->driver)
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
 * FIXME: should registering a new pci device prompt rescan of the bus
 * in order to figure out where we should put this driver? Since we already
 * initialize the PCI bus after we've initialized drivers, we only do probes
 * after the drivers have been loaded...
 *
 * NOTE: This function may fail when we can't find a device for this driver to govern
 * and the driver has specified volatile rescans
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

  driver->device_count = 0;

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

static int pci_dev_read_byte(pci_device_t* device, uint32_t field, uint8_t* out) 
{
  pci_device_address_t* address;

  if (!device)
    return -1;

  address = &device->address;

  return device->raw_ops.read8(address->bus_num, address->device_num, address->func_num, field, out);
}

static int pci_dev_read_word(pci_device_t* device, uint32_t field, uint16_t* out) 
{
  pci_device_address_t* address;

  if (!device)
    return -1;

  address = &device->address;

  return device->raw_ops.read16(address->bus_num, address->device_num, address->func_num, field, out);
}

static int pci_dev_read_dword(pci_device_t* device, uint32_t field, uint32_t* out) 
{
  pci_device_address_t* address;

  if (!device)
    return -1;

  address = &device->address;

  return device->raw_ops.read32(address->bus_num, address->device_num, address->func_num, field, out);
}

/* PCI enumeration stuff */

void enumerate_function(pci_callback_t* callback, pci_bus_t* base_addr,uint8_t bus, uint8_t device, uint8_t func) {

  uint32_t index;

  if (!callback)
    return;
  
  index = base_addr->index;

  pci_device_t identifier = {
    0,
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
      .read_byte = pci_dev_read_byte,
      .read_word = pci_dev_read_word,
      .read_dword = pci_dev_read_dword,
    },
    .bus = base_addr,
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
  identifier.raw_ops.read8(bus, device, func, CAPABILITIES_POINTER, &identifier.capabilities_ptr);

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
  __current_pci_access_impl->read8(bus, device, 0, HEADER_TYPE, &cur_header_type);

  if (!(cur_header_type & 0x80)) {
    return;
  }

  for (uint8_t func = 1; func < 8; func++) {
    enumerate_function(callback, base_addr, bus, device, func);

    /* FIXME: does mcfg cover this?
    uint8_t class, subclass;
    uint8_t sec_bus;

    __current_pci_access_impl->read8(bus, device, func, CLASS, &class);
    __current_pci_access_impl->read8(bus, device, func, SUBCLASS, &subclass);

    // TODO: add bridges here to the bus list?
    if (class == BRIDGE_DEVICE && subclass == PCI_SUBCLASS_BRIDGE_PCI) {

      __current_pci_access_impl->read8(bus, device, func, SECONDARY_BUS, &sec_bus);

      // We've already had this one :clown:
      if (sec_bus >= base_addr->start_bus || sec_bus <= base_addr->end_bus)
        continue;

      enumerate_bus(callback, base_addr, sec_bus);
    }

    */
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

bool register_pci_bridges_from_mcfg(acpi_tbl_mcfg_t* mcfg_ptr) 
{
  pci_bus_t* bridge;
  acpi_parser_t* parser;
  acpi_mcfg_entry_t* c_entry;
  acpi_std_hdr_t* header = &mcfg_ptr->Header;

  get_root_acpi_parser(&parser);

  uint32_t length = header->Length;
  
  if (length == sizeof(acpi_std_hdr_t))
    return false;

  length = ALIGN_UP(length + SMALL_PAGE_SIZE, SMALL_PAGE_SIZE);

  uint32_t entries = (header->Length - sizeof(acpi_tbl_mcfg_t)) / sizeof(acpi_mcfg_entry_t);

  for (uint32_t i = 0; i < entries; i++) {

    c_entry = &mcfg_ptr->Allocations[i];

    uint8_t start = c_entry->StartBusNumber;
    uint8_t end = c_entry->EndBusNumber;
    uint32_t base = c_entry->Address;

    printf("Found PCI bridge (%d -> %d) at 0x%x\n", start, end, base);
  
    // add to pci bridge list
    bridge = create_pci_bus(base, start, end, i, NULL);

    ASSERT_MSG(bridge, "Failed to create PCI bus!");

    list_append(__pci_bridges, bridge);
  }

  __has_registered_bridges = true;
  return true;
}

#define PCI_DECODE_ENABLE (PCI_COMMAND_MEM_SPACE | PCI_COMMAND_IO_SPACE | PCI_COMMAND_BUS_MASTER)

/*!
 * @brief Parse the BAR at the index of @bar and get it's size
 *
 * @bar: the index of the BAR
 */
static size_t __pci_get_bar_size(pci_device_t* device, uint32_t bar)
{
  uintptr_t start32 = 0, size32 = 0;
  uintptr_t start = 0, size = 0, mask = 0;
  uintptr_t buffer_cmd;

  device->ops.read(device, COMMAND, &buffer_cmd);

  /* Disable decode to prevent hardware confusion */
  if (buffer_cmd & PCI_DECODE_ENABLE)
    device->ops.write(device, COMMAND, buffer_cmd & ~PCI_DECODE_ENABLE);

  device->ops.read(device, bar, &start32);
  device->ops.write(device, bar, (uintptr_t)~0);
  device->ops.read(device, bar, &size32);
  device->ops.write(device, bar, start32);

  /* Check for the correct bits */
  if (size32 == ~0 || !size32 || (is_bar_mem(bar) && (size32 & 1) == 1) || (is_bar_io(bar) && (size32 & 2) == 2))
    size32 = 0;

  /* Mask the status bits */
  if (is_bar_io(bar)) {
    start = start32 & PCI_BASE_ADDRESS_IO_MASK;
    size = size32 & PCI_BASE_ADDRESS_IO_MASK;
    mask = (uint32_t)PCI_BASE_ADDRESS_IO_MASK;
  } else if (is_bar_mem(bar)) {
    start = start32 & PCI_BASE_ADDRESS_MEM_MASK;
    size = size32 & PCI_BASE_ADDRESS_MEM_MASK;
    mask = (uint32_t)PCI_BASE_ADDRESS_MEM_MASK;
  }

  if (is_bar_64bit(bar)) {
    device->ops.read(device, bar + 4, &start32);
    device->ops.write(device, bar + 4, ~0);
    device->ops.read(device, bar + 4, &size32);
    device->ops.write(device, bar + 4, start32);

    start |= ((uintptr_t)start32 << 32);
    size |= ((uintptr_t)size32 << 32);
  }

  if (buffer_cmd & PCI_DECODE_ENABLE)
    device->ops.write(device, COMMAND, buffer_cmd);

  if (!size)
    return 0;

  uintptr_t final_size = mask & size;

  if (!final_size)
    return 0;

  final_size = final_size & ~(final_size-1);

  if (start == size && ((start | (final_size - 1)) & mask) != mask)
    return 0;

  return final_size;
}

/*!
 * @brief Get the BAR size at @index
 *
 * Nothing to add here...
 */
size_t pci_get_bar_size(pci_device_t* device, uint32_t index)
{
  uint32_t bar = BAR0 + (index << 2);

  /* Only BAR 0 to 6 */
  if (bar > ROM_ADDRESS)
    return 0;

  return __pci_get_bar_size(device, BAR0 + (index << 2));
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

static uint8_t __pci_find_cap(pci_device_t* device, uint8_t start, uint32_t cap)
{
  uint16_t value;
  uint8_t pos = start;

  device->ops.read_byte(device, pos, &pos);

  /* NOTE: 48 is the pci cap TTL */
  for (uintptr_t i = 0; i < 48; i++) {
    /* pos can't be in the standard header :clown: */
    if (pos < 0x40)
      break;

    pos &= ~3;

    device->ops.read_word(device, pos, &value);

    if ((value & 0xff) == 0xff)
      break;

    if ((value & 0xff) == cap)
      return pos;

    pos = (value >> 8);
  }

  return 0;
}

static uint8_t pci_find_cap_start_offset(pci_device_t* device) 
{
  uint16_t status;

  device->ops.read_word(device, STATUS, &status);

  if (!(status & PCI_STATUS_CAP_LIST))
    return 0;

  switch (device->header_type) {
    case PCI_HDR_TYPE_NORMAL:
    case PCI_HDR_TYPE_BRIDGE:
      return CAPABILITIES_POINTER;
    case PCI_HDR_TYPE_CARDBUS:
      /* TODO: support cardbusses */
      return 0;
  }

  return 0;
}

/*!
 * @brief Walks the capabilities list of a pci device and finds the position of a certain capability in the pci config space
 *
 * Nothing to add here...
 */
uint8_t pci_find_cap(pci_device_t* device, uint32_t cap)
{
  uint8_t start;

  start = pci_find_cap_start_offset(device);

  if (start)
    start = __pci_find_cap(device, start, cap);

  return start;
}


/*!
 * @brief Enable PCI device for sw use
 *
 * Nothing to add here...
 */
int pci_device_enable(pci_device_t* device)
{
  /* TODO: power management */

  if (device->enabled_count)
    return 0;

  /* Set IO and memory to respond */
  pci_set_memory(&device->address, true);
  pci_set_io(&device->address, true);

  /* DMA bus mastering (FIXME: should we always turn this on?) */
  pci_set_bus_mastering(&device->address, true);

  device->enabled_count++;
  
  return 0;
}

int pci_device_disable(pci_device_t* device)
{
  if (!device->enabled_count)
    return 0;

  device->enabled_count--;

  /* Only disable once we've reached the end here */
  if (!device->enabled_count) {

    /* Set IO and memory to off */
    pci_set_memory(&device->address, false);
    pci_set_io(&device->address, false);

    /* DMA bus mastering (FIXME: see pci_device_enable) */
    pci_set_bus_mastering(&device->address, false);

    /* Also interrupts, just in case */
    pci_set_interrupt_line(&device->address, false);
  }

  return 0;
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

void init_pci_early()
{
  if (test_pci_io_type1()) {
    __current_addressing_mode = PCI_IOPORT_ACCESS;
    __current_pci_access_impl = &g_pci_type1_impl;
  } else if (test_pci_io_type2()) {
    kernel_panic("Detected PCI type 2 interfacing! currently not supported.");
    __current_pci_access_impl = &g_pci_type2_impl;
  } else {
    kernel_panic("No viable pci access method found");
  }

}

bool init_pci() 
{
  __pci_drivers = nullptr;
  __pci_devices = init_list();
  __pci_bridges = init_list();

  __pci_dev_allocator = create_zone_allocator(28 * Kib, sizeof(pci_device_t), NULL);

  init_pci_bus();

  acpi_tbl_mcfg_t* mcfg = find_acpi_table(ACPI_SIG_MCFG, sizeof(acpi_tbl_mcfg_t));

  if (!mcfg)
    kernel_panic("no mcfg found!");

  ASSERT_MSG(register_pci_bridges_from_mcfg(mcfg), "Unable to register pci bridges from mcfg! (ACPI)");

  return true;
}

pci_device_t create_pci_device(struct pci_bus* bus) 
{
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
 * Pci configuration routines
 *
 * TODO: Since the type 1 legacy access mode does not support segment groups, we don't do anything
 * with that right now, but we should be able to detect wether or not we're using ECAM as our configuration-
 * space handler. We should look into adding that as a pci_access impl
 *
 * Also __current_addressing_mode is kinda weird, since it's different per PCI device/function.
 */

int pci_read32(uint32_t segment, uint32_t bus, uint32_t device, uint32_t func, uint32_t reg, uint32_t *value)
{
  if (!__current_pci_access_impl)
    return -1;

  return __current_pci_access_impl->read32(bus, device, func, reg, value);
}

int pci_read16(uint32_t segment, uint32_t bus, uint32_t device, uint32_t func, uint32_t reg, uint16_t *value)
{
  if (!__current_pci_access_impl)
    return -1;

  return __current_pci_access_impl->read16(bus, device, func, reg, value);
}

int pci_read8(uint32_t segment, uint32_t bus, uint32_t device, uint32_t func, uint32_t reg, uint8_t *value)
{
  if (!__current_pci_access_impl)
    return -1;

  return __current_pci_access_impl->read8(bus, device, func, reg, value);
}

int pci_write32(uint32_t segment, uint32_t bus, uint32_t device, uint32_t func, uint32_t reg, uint32_t value)
{
  if (!__current_pci_access_impl)
    return -1;

  return __current_pci_access_impl->write32(bus, device, func, reg, value);
}

int pci_write16(uint32_t segment, uint32_t bus, uint32_t device, uint32_t func, uint32_t reg, uint16_t value)
{
  if (!__current_pci_access_impl)
    return -1;

  return __current_pci_access_impl->write16(bus, device, func, reg, value);
}

int pci_write8(uint32_t segment, uint32_t bus, uint32_t device, uint32_t func, uint32_t reg, uint8_t value)
{
  if (!__current_pci_access_impl)
    return -1;

  return __current_pci_access_impl->write8(bus, device, func, reg, value);
}

/* PCI READ/WRITE WRAPPERS */
// TODO

static ALWAYS_INLINE void pci_send_command(pci_device_address_t* address, bool or_and, uint32_t shift) 
{
  uint16_t placeback;
  __current_pci_access_impl->read16(address->bus_num, address->device_num, address->func_num, COMMAND, &placeback);

  if (or_and) {
    __current_pci_access_impl->write16(address->bus_num, address->device_num, address->func_num, COMMAND, placeback | shift);
  } else {
    __current_pci_access_impl->write16(address->bus_num, address->device_num, address->func_num, COMMAND, placeback & ~(shift));
  }
}

void pci_set_io(pci_device_address_t* address, bool value)
{
  pci_send_command(address, value, PCI_COMMAND_IO_SPACE);
}

void pci_set_memory(pci_device_address_t* address, bool value)
{
  pci_send_command(address, value, PCI_COMMAND_MEM_SPACE);
}

void pci_set_interrupt_line(pci_device_address_t* address, bool value) 
{
  pci_send_command(address, !value, PCI_COMMAND_INT_DISABLE);
}

void pci_set_bus_mastering(pci_device_address_t* address, bool value) 
{
  pci_send_command(address, value, PCI_COMMAND_BUS_MASTER);
}

pci_accessmode_t get_current_addressing_mode() {
  return __current_addressing_mode;
}
