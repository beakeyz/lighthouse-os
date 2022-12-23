#ifndef __LIGHT_PCI__
#define __LIGHT_PCI__
#include "libk/linkedlist.h"
#include <libk/stddef.h>

#define PCI_PORT_ADDR 0xCF8
#define PCI_PORT_VALUE 0xCFC

#define PCI_MAX_BUSSES 256
#define PCI_MAX_DEV_PER_BUS 32

#define GET_BUS_NUM(device_num) (uint8_t)(device_num >> 16)
#define GET_SLOT_NUM(device_num) (uint8_t)(device_num >> 8)
#define GET_FUNC_NUM(device_num) (uint8_t)(device_num)

#define GET_PCI_ADDR(dev_num, field) (uint32_t)(0x80000000 | (GET_BUS_NUM(dev_num) << 16) | (GET_SLOT_NUM(dev_num) << 11) | (GET_FUNC_NUM(dev_num) << 8) | (field & 0xFC))
#define GET_PCIE_ADDR(dev_num, field) (uintptr_t)((GET_BUS_NUM(device_num) << 20) | (GET_SLOT_NUM(device_num) << 15) | (GET_FUNC_NUM(device_num) << 12) | (field))

struct DeviceIdentifier;
struct PCI_Bridge; 

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
} DeviceIdentifier_t;

typedef struct PCI_Bridge {
  uint8_t start_bus;
  uint8_t end_bus;
  uint32_t base_addr;
  uint32_t index;
} __attribute__((packed)) PCI_Bridge_t;

extern list_t g_pci_bridges;
extern bool g_has_registered_bridges;

void print_device_info(DeviceIdentifier_t* dev);

void enumerate_function(uint64_t base_addr, uint64_t func, PCI_FUNC_ENUMERATE_CALLBACK callback);
void enumerate_devices(uint64_t base_addr, uint64_t device);
void enumerate_bus(uint64_t base_addr, uint64_t bus);
void enumerate_bridges(PCI_ENUMERATE_CALLBACK callback);

bool register_pci_bridges_from_mcfg(uintptr_t mcfg_ptr);

uint32_t pci_field_read (uint32_t device_num, uint32_t field, uint32_t size);
void pci_field_write (uint32_t device_num, uint32_t field, uint32_t size, uint32_t val);

bool test_pci_io ();
// pci scanning (I would like this to be as advanced as possible and not some idiot simple thing)


#endif // !__
