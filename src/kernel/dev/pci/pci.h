#ifndef __LIGHT_PCI__
#define __LIGHT_PCI__
#include "dev/pci/bridge.h"
#include "libk/linkedlist.h"
#include <libk/stddef.h>

#define PCI_PORT_ADDR 0xCF8
#define PCI_PORT_VALUE 0xCFC

#define PCI_NONE_VALUE 0xffff

#define PCI_MAX_BUSSES 256
#define PCI_MAX_DEV_PER_BUS 32

#define PCI_32BIT_PORT_SIZE 0x4
#define PCI_16BIT_PORT_SIZE 0x2
#define PCI_8BIT_PORT_SIZE  0x1

#define GET_BUS_NUM(device_num) (uint8_t)(device_num >> 16)
#define GET_SLOT_NUM(device_num) (uint8_t)(device_num >> 8)
#define GET_FUNC_NUM(device_num) (uint8_t)(device_num)

#define GET_PCI_ADDR(dev_num, field) (uint32_t)(0x80000000 | (GET_BUS_NUM(dev_num) << 16) | (GET_SLOT_NUM(dev_num) << 11) | (GET_FUNC_NUM(dev_num) << 8) | (field & 0xFC))
#define GET_PCIE_ADDR(dev_num, field) (uintptr_t)((GET_BUS_NUM(device_num) << 20) | (GET_SLOT_NUM(device_num) << 15) | (GET_FUNC_NUM(device_num) << 12) | (field))

struct DeviceIdentifier;
struct DeviceAddress;
struct PCI_Bridge; 

typedef enum PciRegisterOffset {
  VENDOR_ID = 0x00,            // word
  DEVICE_ID = 0x02,            // word
  COMMAND = 0x04,              // word
  STATUS = 0x06,               // word
  REVISION_ID = 0x08,          // byte
  PROG_IF = 0x09,              // byte
  SUBCLASS = 0x0a,             // byte
  CLASS = 0x0b,                // byte
  CACHE_LINE_SIZE = 0x0c,      // byte
  LATENCY_TIMER = 0x0d,        // byte
  HEADER_TYPE = 0x0e,          // byte
  BIST = 0x0f,                 // byte
  BAR0 = 0x10,                 // u32
  BAR1 = 0x14,                 // u32
  BAR2 = 0x18,                 // u32
  SECONDARY_BUS = 0x19,        // byte
  BAR3 = 0x1C,                 // u32
  BAR4 = 0x20,                 // u32
  BAR5 = 0x24,                 // u32
  SUBSYSTEM_VENDOR_ID = 0x2C,  // u16
  SUBSYSTEM_ID = 0x2E,         // u16
  CAPABILITIES_POINTER = 0x34, // u8
  INTERRUPT_LINE = 0x3C,       // byte
  INTERRUPT_PIN = 0x3D,        // byte
} PciRegisterOffset_t;

typedef void* (*PCI_FUNC) (
  uint32_t dev,
  uint16_t vendor_id,
  uint16_t dev_id,
  void* stuff
);

typedef void (*PCI_ENUMERATE_CALLBACK) (
  uint64_t base_addr,
  uint64_t next_level
);

typedef void (*PCI_FUNC_ENUMERATE_CALLBACK) (
  struct DeviceIdentifier* identifier
);

typedef struct PciFuncCallback {
  PCI_FUNC_ENUMERATE_CALLBACK callback;
  const char* callback_name;
} PciFuncCallback_t;

typedef struct DeviceAddress {
  uint32_t index;
  uint32_t bus_num;
  uint32_t device_num;
  uint32_t func_num;
} DeviceAddress_t;

typedef struct DeviceIdentifier {
  uint16_t vendor_id;
  uint16_t dev_id;
  uint16_t command;
  uint16_t status;
  uint8_t revision_id;
  uint8_t prog_if;
  uint8_t subclass;
  uint8_t class;
  uint8_t cachelinesize;
  uint8_t latency_timer;
  uint8_t header_type;
  uint8_t BIST;
  uint8_t capabilities_ptr;
  DeviceAddress_t address;
} DeviceIdentifier_t;

extern list_t* g_pci_bridges;
extern list_t* g_pci_devices;
extern bool g_has_registered_bridges;
extern PciFuncCallback_t current_active_callback;

bool init_pci();
bool set_pci_func(PCI_FUNC_ENUMERATE_CALLBACK callback, const char* name);
bool set_current_enum_func(PciFuncCallback_t new_callback);

void print_device_info(DeviceIdentifier_t* dev);

void register_pci_devices(DeviceIdentifier_t* dev);

void enumerate_function(PCI_Bridge_t* base_addr, uint8_t bus, uint8_t device, uint8_t func, PCI_FUNC_ENUMERATE_CALLBACK callback);
void enumerate_devices(PCI_Bridge_t* base_addr, uint8_t bus, uint8_t device);
void enumerate_bus(PCI_Bridge_t* base_addr, uint8_t bus);
void enumerate_bridges();
void enumerate_pci_raw(PciFuncCallback_t callback);
void enumerate_registerd_devices(PCI_FUNC_ENUMERATE_CALLBACK callback);

bool register_pci_bridges_from_mcfg(uintptr_t mcfg_ptr);

//uint32_t pci_field_read (uint32_t device_num, uint32_t field, uint32_t size);
//void pci_field_write (uint32_t device_num, uint32_t field, uint32_t size, uint32_t val);
uint32_t pci_field_read (uint32_t device_num, uint32_t field, uint32_t size);
void pci_field_write (uint32_t device_num, uint32_t field, uint32_t size, uint32_t val);

PCI_Bridge_t* get_bridge_by_index(uint32_t bridge_index);

void pci_write_32(DeviceAddress_t* address, uint32_t field, uint32_t value);
void pci_write_16(DeviceAddress_t* address, uint32_t field, uint16_t value);
void pci_write_8(DeviceAddress_t* address, uint32_t field, uint8_t value);

uint32_t pci_read_32(DeviceAddress_t* address, uint32_t field);
uint16_t pci_read_16(DeviceAddress_t* address, uint32_t field);
uint8_t pci_read_8(DeviceAddress_t* address, uint32_t field);

bool test_pci_io ();
// pci scanning (I would like this to be as advanced as possible and not some idiot simple thing)


#endif // !__
