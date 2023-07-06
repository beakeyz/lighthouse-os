#ifndef __ANIVA_PCI__
#define __ANIVA_PCI__
#include "libk/data/linkedlist.h"
#include "dev/driver.h"
#include "system/acpi/structures.h"
#include <libk/stddef.h>

#define PCI_PORT_ADDR 0xCF8
#define PCI_PORT_VALUE 0xCFC

#define PCI_NONE_VALUE 0xffff

#define PCI_MAX_BUSSES 256
#define PCI_MAX_DEV_PER_BUS 32

#define PCI_32BIT_PORT_SIZE 0x4
#define PCI_16BIT_PORT_SIZE 0x2
#define PCI_8BIT_PORT_SIZE  0x1

#define PCI_CONF1_BUS_SHIFT	16 /* Bus number */
#define PCI_CONF1_DEV_SHIFT	11 /* Device number */
#define PCI_CONF1_FUNC_SHIFT	8  /* Function number */

#define PCI_CONF1_BUS_MASK	0xff
#define PCI_CONF1_DEV_MASK	0x1f
#define PCI_CONF1_FUNC_MASK	0x7
#define PCI_CONF1_REG_MASK	0xfc /* Limit aligned offset to a maximum of 256B */

#define PCI_CONF1_ENABLE	(1 << 31)
#define PCI_CONF1_BUS(x)	(((x) & PCI_CONF1_BUS_MASK) << PCI_CONF1_BUS_SHIFT)
#define PCI_CONF1_DEV(x)	(((x) & PCI_CONF1_DEV_MASK) << PCI_CONF1_DEV_SHIFT)
#define PCI_CONF1_FUNC(x)	(((x) & PCI_CONF1_FUNC_MASK) << PCI_CONF1_FUNC_SHIFT)
#define PCI_CONF1_REG(x)	((x) & PCI_CONF1_REG_MASK)

#define PCI_CONF1_ADDRESS(bus, dev, func, reg) \
	(PCI_CONF1_ENABLE | \
	 PCI_CONF1_BUS(bus) | \
	 PCI_CONF1_DEV(dev) | \
	 PCI_CONF1_FUNC(func) | \
	 PCI_CONF1_REG(reg))


#define PCI_CONF1_EXT_REG_SHIFT	16
#define PCI_CONF1_EXT_REG_MASK	0xf00
#define PCI_CONF1_EXT_REG(x)	(((x) & PCI_CONF1_EXT_REG_MASK) << PCI_CONF1_EXT_REG_SHIFT)

#define PCI_CONF1_EXT_ADDRESS(bus, dev, func, reg) \
	(PCI_CONF1_ADDRESS(bus, dev, func, reg) | \
	 PCI_CONF1_EXT_REG(reg))

struct pci_device_identifier;
struct DeviceAddress;
struct pci_bus;
struct pci_device;
struct raw_pci_impls;

typedef enum PciAccessMode {
  PCI_IOPORT_ACCESS = 0,
  PCI_MEM_ACCESS,
  PCI_UNKNOWN_ACCESS,
} PciAccessMode_t;

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
  struct pci_device_identifier* identifier
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

#define PCI_IMPLS_ERROR 0
#define PCI_IMPLS_SUCCESS 1

typedef struct pci_device_impls {
  int (*write32)(uint32_t bus, uint32_t dev, uint32_t func, uint32_t field, uint32_t value);
  int (*write16)(uint32_t bus, uint32_t dev, uint32_t func, uint32_t field, uint16_t value);
  int (*write8)(uint32_t bus, uint32_t dev, uint32_t func, uint32_t field, uint8_t value);
  int (*read32)(uint32_t bus, uint32_t dev, uint32_t func, uint32_t field, uint32_t* value);
  int (*read16)(uint32_t bus, uint32_t dev, uint32_t func, uint32_t field, uint16_t* value);
  int (*read8)(uint32_t bus, uint32_t dev, uint32_t func, uint32_t field, uint8_t* value);
//  void* (*map)(struct pci_bus* bus, uint32_t device_function, int where);
} pci_device_impls_t;

typedef struct pci_device_identifier {
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
  uint8_t interrupt_line;
  uint8_t interrupt_pin;
  uint8_t capabilities_ptr;
  pci_device_impls_t ops;
  DeviceAddress_t address;
} pci_device_identifier_t;

typedef struct pci_driver {
  aniva_driver_t* parent;
  const pci_device_identifier_t identifier;
  int (*f_pci_on_shutdown)(struct pci_device* device);
  int (*f_pci_on_remove)(struct pci_device* device);
  int (*f_pci_resume)(struct pci_device* device);
  int (*f_pci_suspend)(struct pci_device* device);
} pci_driver_t;

typedef struct pci_device {
  struct pci_bus *m_bus;
  pci_device_impls_t m_impl;
  pci_device_identifier_t m_identifier;

  uint8_t m_pcie_cap;
  uint8_t m_msi_cap;

  // inline enum?? :thinking:
  enum {
    D0,
    D1,
    D2,
    D3_HOT,
    D3_COLD,
    PCI_UNKNOWN,
    PCI_POWER_ERROR
  } m_device_state;

  pci_driver_t *m_driver;

  uint64_t m_dma_mask;
  uint32_t m_imm_support:1;

} pci_device_t;

extern list_t* g_pci_bridges;
extern list_t* g_pci_devices;
extern bool g_has_registered_bridges;
extern PciFuncCallback_t current_active_callback;

PciAccessMode_t get_current_addressing_mode();

pci_device_t create_pci_device(struct pci_bus* bus);
pci_driver_t get_pci_device_host_bridge(struct pci_device* device);

bool init_pci();
bool set_pci_func(PCI_FUNC_ENUMERATE_CALLBACK callback, const char* name);
bool set_current_enum_func(PciFuncCallback_t new_callback);

void print_device_info(pci_device_identifier_t* dev);

void register_pci_devices(pci_device_identifier_t* dev);

void enumerate_function(struct pci_bus* base_addr, uint8_t bus, uint8_t device, uint8_t func, PCI_FUNC_ENUMERATE_CALLBACK callback);
void enumerate_device(struct pci_bus* base_addr, uint8_t bus, uint8_t device);
void enumerate_bus(struct pci_bus* base_addr, uint8_t bus);
void enumerate_bridges();
void enumerate_pci_raw(PciFuncCallback_t callback);
void enumerate_registerd_devices(PCI_FUNC_ENUMERATE_CALLBACK callback);

bool register_pci_bridges_from_mcfg(acpi_mcfg_t* mcfg_ptr);

struct pci_bus* get_bridge_by_index(uint32_t bridge_index);

void pci_write_32(DeviceAddress_t* address, uint32_t field, uint32_t value);
void pci_write_16(DeviceAddress_t* address, uint32_t field, uint16_t value);
void pci_write_8(DeviceAddress_t* address, uint32_t field, uint8_t value);

uint32_t pci_read_32(DeviceAddress_t* address, uint32_t field);
uint16_t pci_read_16(DeviceAddress_t* address, uint32_t field);
uint8_t pci_read_8(DeviceAddress_t* address, uint32_t field);

bool test_pci_io_type1();
bool test_pci_io_type2();
// pci scanning (I would like this to be as advanced as possible and not some idiot simple thing)
// uhm what?

void pci_set_io(DeviceAddress_t* address, bool value);
void pci_set_memory(DeviceAddress_t* address, bool value);
void pci_set_interrupt_line(DeviceAddress_t* address, bool value);
void pci_set_bus_mastering(DeviceAddress_t* address, bool value);

extern struct raw_pci_impls* early_raw_pci_impls;

#endif // !__
