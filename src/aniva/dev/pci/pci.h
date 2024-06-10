#ifndef __ANIVA_PCI__
#define __ANIVA_PCI__
#include "dev/driver.h"
#include "dev/pci/io.h"
#include "sync/mutex.h"
#include "system/acpi/tables.h"
#include <libk/stddef.h>
#include <system/resource.h>

#define PCI_PORT_ADDR 0xCF8
#define PCI_PORT_VALUE 0xCFC

#define PCI_NONE_VALUE 0xffff

#define PCI_MAX_BUSSES 256
#define PCI_MAX_DEV_PER_BUS 32

#define PCI_32BIT_PORT_SIZE 0x4
#define PCI_16BIT_PORT_SIZE 0x2
#define PCI_8BIT_PORT_SIZE 0x1

#define PCI_CONF1_BUS_SHIFT 16 /* Bus number */
#define PCI_CONF1_DEV_SHIFT 11 /* Device number */
#define PCI_CONF1_FUNC_SHIFT 8 /* Function number */

#define PCI_CONF1_BUS_MASK 0xff
#define PCI_CONF1_DEV_MASK 0x1f
#define PCI_CONF1_FUNC_MASK 0x7
#define PCI_CONF1_REG_MASK 0xfc /* Limit aligned offset to a maximum of 256B */

#define PCI_CONF1_ENABLE (1 << 31)
#define PCI_CONF1_BUS(x) (((x) & PCI_CONF1_BUS_MASK) << PCI_CONF1_BUS_SHIFT)
#define PCI_CONF1_DEV(x) (((x) & PCI_CONF1_DEV_MASK) << PCI_CONF1_DEV_SHIFT)
#define PCI_CONF1_FUNC(x) (((x) & PCI_CONF1_FUNC_MASK) << PCI_CONF1_FUNC_SHIFT)
#define PCI_CONF1_REG(x) ((x) & PCI_CONF1_REG_MASK)

#define PCI_CONF1_ADDRESS(bus, dev, func, reg) \
    (PCI_CONF1_ENABLE | PCI_CONF1_BUS(bus) | PCI_CONF1_DEV(dev) | PCI_CONF1_FUNC(func) | PCI_CONF1_REG(reg))

#define PCI_CONF1_EXT_REG_SHIFT 16
#define PCI_CONF1_EXT_REG_MASK 0xf00
#define PCI_CONF1_EXT_REG(x) (((x) & PCI_CONF1_EXT_REG_MASK) << PCI_CONF1_EXT_REG_SHIFT)

#define PCI_CONF1_EXT_ADDRESS(bus, dev, func, reg) \
    (PCI_CONF1_ADDRESS(bus, dev, func, reg) | PCI_CONF1_EXT_REG(reg))

struct pci_device_address;
struct pci_bus;
struct pci_driver;
struct pci_device;
struct device;
struct raw_pci_impls;

typedef enum pci_accessmode {
    PCI_IOPORT_ACCESS = 0,
    PCI_MEM_ACCESS,
    PCI_UNKNOWN_ACCESS,
} pci_accessmode_t;

typedef enum pci_register_offset {
    VENDOR_ID = 0x00, // word
    DEVICE_ID = 0x02, // word
    COMMAND = 0x04, // word
    STATUS = 0x06, // word
    REVISION_ID = 0x08, // byte
    PROG_IF = 0x09, // byte
    SUBCLASS = 0x0a, // byte
    CLASS = 0x0b, // byte
    CACHE_LINE_SIZE = 0x0c, // byte
    LATENCY_TIMER = 0x0d, // byte
    HEADER_TYPE = 0x0e, // byte
    BIST = 0x0f, // byte
    BAR0 = 0x10, // u32
    BAR1 = 0x14, // u32
    BAR2 = 0x18, // u32
    SECONDARY_BUS = 0x19, // byte
    BAR3 = 0x1C, // u32
    BAR4 = 0x20, // u32
    BAR5 = 0x24, // u32
    SUBSYSTEM_VENDOR_ID = 0x2C, // u16
    SUBSYSTEM_ID = 0x2E, // u16
    ROM_ADDRESS = 0x30, // u32
    CAPABILITIES_POINTER = 0x34, // u8
    INTERRUPT_LINE = 0x3C, // byte
    INTERRUPT_PIN = 0x3D, // byte
    MIN_GNT = 0x3e, // byte
    MAX_LAT = 0x3f, // byte

    FIRST_REGISTER = VENDOR_ID,
    LAST_REGISTER = MAX_LAT,
} pci_register_offset_t;

bool pci_register_is_std(pci_register_offset_t offset);
uintptr_t get_pci_register_size(pci_register_offset_t offset);

/*
 * These functions take the value that is found at the
 * base address of a pci header (BAR)
 */
uintptr_t get_bar_address(uint32_t bar);
bool is_bar_io(uint32_t bar);
bool is_bar_mem(uint32_t bar);
uint8_t bar_get_type(uint32_t bar);
bool is_bar_16bit(uint32_t bar);
bool is_bar_32bit(uint32_t bar);
bool is_bar_64bit(uint32_t bar);

size_t pci_get_bar_size(struct pci_device* dev, uint32_t bar);

/*
 * As specified in linux/pci_regs.h, there are 256 bytes of configuration space, of which
 * 64 bytes are standardised
 */
#define PCI_STD_BAR_COUNT 6
#define PCI_STD_HEADER_SIZE 64

/*
 * This structure is yoinked from linux in order to give a
 * consistent representation of the resource layout and
 * numbering
 */
enum {
    /* #0-5: standard PCI resources */
    PCI_STD_RESOURCES,
    PCI_STD_RESOURCE_END = PCI_STD_RESOURCES + 6 - 1,

    /* #6: expansion ROM resource */
    PCI_ROM_RESOURCE,

/* PCI-to-PCI (P2P) bridge windows */
#define PCI_BRIDGE_IO_WINDOW (PCI_BRIDGE_RESOURCES + 0)
#define PCI_BRIDGE_MEM_WINDOW (PCI_BRIDGE_RESOURCES + 1)
#define PCI_BRIDGE_PREF_MEM_WINDOW (PCI_BRIDGE_RESOURCES + 2)

/* CardBus bridge windows */
#define PCI_CB_BRIDGE_IO_0_WINDOW (PCI_BRIDGE_RESOURCES + 0)
#define PCI_CB_BRIDGE_IO_1_WINDOW (PCI_BRIDGE_RESOURCES + 1)
#define PCI_CB_BRIDGE_MEM_0_WINDOW (PCI_BRIDGE_RESOURCES + 2)
#define PCI_CB_BRIDGE_MEM_1_WINDOW (PCI_BRIDGE_RESOURCES + 3)

/* Total number of bridge resources for P2P and CardBus */
#define PCI_BRIDGE_RESOURCE_NUM 4

    /* Resources assigned to buses behind the bridge */
    PCI_BRIDGE_RESOURCES,
    PCI_BRIDGE_RESOURCE_END = PCI_BRIDGE_RESOURCES + PCI_BRIDGE_RESOURCE_NUM - 1,

    /* Total resources associated with a PCI device */
    PCI_NUM_RESOURCES,
};

typedef void* (*PCI_FUNC)(
    uint32_t dev,
    uint16_t vendor_id,
    uint16_t dev_id,
    void* stuff);

typedef void (*PCI_ENUMERATE_CALLBACK)(
    uint64_t base_addr,
    uint64_t next_level);

typedef void (*PCI_FUNC_ENUMERATE_CALLBACK)(
    struct pci_device* device);

typedef struct pci_callback {
    PCI_FUNC_ENUMERATE_CALLBACK callback;
    const char* callback_name;
} pci_callback_t;

typedef struct pci_device_address {
    uint32_t index;
    uint32_t bus_num;
    uint32_t device_num;
    uint32_t func_num;
} pci_device_address_t;

typedef struct pci_device_ops {
    int (*write)(struct pci_device* dev, uint32_t field, uintptr_t value);
    int (*read)(struct pci_device* dev, uint32_t field, uintptr_t* value);
    int (*read_byte)(struct pci_device* dev, uint32_t field, uint8_t* out);
    int (*read_word)(struct pci_device* dev, uint32_t field, uint16_t* out);
    int (*read_dword)(struct pci_device* dev, uint32_t field, uint32_t* out);
} pci_device_ops_t;

void init_pci_early();
bool init_pci();
void init_pci_drivers();

int unregister_unused_pci_drivers();

typedef struct pci_device {
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
    uint8_t interrupt_pin; /* NOTE: the interrupt fields are writable, but that should happen with the config_write functions */
    uint8_t capabilities_ptr;
    /* These fields are copies of the values that are located in the actual header */

    pci_device_ops_t ops;
    pci_device_ops_io_t raw_ops;
    pci_device_address_t address;

    /* These resources can be both I/O and memory ranges */
    kresource_bundle_t* resources[PCI_NUM_RESOURCES];

    struct pci_bus* bus;
    struct pci_driver* driver;
    struct device* dev;

    // inline enum?? :thinking:
    enum {
        D0,
        D1,
        D2,
        D3_HOT,
        D3_COLD,
        PCI_UNKNOWN,
        PCI_POWER_ERROR
    } device_state;

    uint64_t dma_mask;

    /* Might need to be atomic */
    uint64_t enabled_count;
} pci_device_t;

void pci_device_register_resource(pci_device_t* device, uint32_t index);
void pci_device_unregister_resource(pci_device_t* device, uint32_t index);

void enumerate_function(pci_callback_t* callback, struct pci_bus* base_addr, uint8_t bus, uint8_t device, uint8_t func);
void enumerate_device(pci_callback_t* callback, struct pci_bus* base_addr, uint8_t bus, uint8_t device);
void enumerate_bus(pci_callback_t* callback, struct pci_bus* base_addr, uint8_t bus);
void enumerate_bridges(pci_callback_t* callback);
void enumerate_registerd_devices(PCI_FUNC_ENUMERATE_CALLBACK callback);

bool register_pci_bridges_from_mcfg(acpi_tbl_mcfg_t* mcfg_ptr);

uint8_t pci_find_cap(pci_device_t* device, uint32_t cap);

int pci_device_enable(pci_device_t* device);
int pci_device_disable(pci_device_t* device);

#define PCI_DEVID_USE_VENDOR_ID (1 << 0)
#define PCI_DEVID_USE_DEVICE_ID (1 << 1)
#define PCI_DEVID_USE_SUB_VENDOR_ID (1 << 2)
#define PCI_DEVID_USE_SUB_DEVICE_ID (1 << 3)

#define PCI_DEVID_USE_CLASS (1 << 4)
#define PCI_DEVID_USE_SUBCLASS (1 << 5)
#define PCI_DEVID_USE_PROG_IF (1 << 6)
#define PCI_DEVID_RESERVED_BIT (1 << 7)

#define PCI_DEVID_ANY_VALUE (~0)

#define PCI_DEVID_USE_ALL 0xFF
#define PCI_DEVID_USE_IDS (uint8_t)(PCI_DEVID_USE_VENDOR_ID | PCI_DEVID_USE_DEVICE_ID | PCI_DEVID_USE_SUB_VENDOR_ID | PCI_DEVID_USE_SUB_DEVICE_ID)
#define PCI_DEVID_USE_CLASSES (uint8_t)(PCI_DEVID_USE_CLASS | PCI_DEVID_USE_SUBCLASS | PCI_DEVID_USE_PROG_IF)

#define PCI_DEVID_SHOULD(id, bit) (((id) & (bit)) == (bit))

typedef struct pci_dev_id {

    struct {
        uint16_t vendor_id;
        uint16_t device_id;
        uint16_t sub_vendor;
        uint16_t sub_device;
    } ids;

    struct {
        uint8_t class;
        uint8_t subclass;
        uint8_t prog_if;
    } classes;

    uint8_t usage_bits;
} pci_dev_id_t;

bool is_end_devid(pci_dev_id_t* dev_id);

#define FOREACH_PCI_DEVID(i, devids) for (pci_dev_id_t* i = devids; !is_end_devid(i); i++)

#define PCI_DEVID_CLASSES(class, subclass, prog_if)         \
    {                                                       \
        {                                                   \
            0,                                              \
        },                                                  \
        { class, subclass, prog_if }, PCI_DEVID_USE_CLASSES \
    }
#define PCI_DEVID_IDS(vendor, device, sub_vendor, sub_device) \
    {                                                         \
        { vendor, device, sub_vendor, sub_device }, {         \
                                                        0,    \
                                                    },        \
        PCI_DEVID_USE_IDS                                     \
    }

#define PCI_DEVID_CLASSES_EX(class, subclass, prog_if, bits) \
    {                                                        \
        {                                                    \
            0,                                               \
        },                                                   \
        { class, subclass, prog_if }, (bits)                 \
    }
#define PCI_DEVID_IDS_EX(vendor, device, sub_vendor, sub_device, bits) \
    {                                                                  \
        { vendor, device, sub_vendor, sub_device }, {                  \
                                                        0,             \
                                                    },                 \
        (bits)                                                         \
    }

#define PCI_DEVID_FULL(vendor, device, sub_vendor, sub_device, class, subclass, prog_if)            \
    {                                                                                               \
        { vendor, device, sub_vendor, sub_device }, { class, subclass, prog_if }, PCI_DEVID_USE_ALL \
    }
#define PCI_DEVID(vendor, device, sub_vendor, sub_device, class, subclass, prog_if, bits) \
    {                                                                                     \
        { vendor, device, sub_vendor, sub_device }, { class, subclass, prog_if }, bits    \
    }
#define PCI_DEVID_END \
    {                 \
        {             \
            0,        \
        },            \
        {             \
            0,        \
        },            \
        0             \
    }

#define PCI_DEV_FLAG_NO_RESCAN 0x01 /* Should we rewalk the found pci devices once this driver is registered? (Handy for internal drivers) */
#define PCI_DEV_FLAG_VOLATILE_RESCAN 0x02 /* Should the rescan return failure when this pci driver is registered? */

/*
 *
 */
typedef struct pci_driver {
    /* Amount of devices this driver is giving service to */
    uint8_t device_count; // NOTE: we are wasting a bunch of space here. For any 8-, 16- or 32 bit numbers place them below here
    uint8_t device_flags;

    /* Lock that ensures only one opperation at a time on this driver */
    mutex_t* lock;

    /*
     * Functions that are called for specific PCI housekeeping
     */

    /* NOTE: Probe is mostly called while the drivers lock is held */
    int (*f_probe)(struct pci_device* device, struct pci_driver* driver);
    int (*f_pci_on_shutdown)(struct pci_device* device);
    int (*f_pci_on_remove)(struct pci_device* device);
    int (*f_pci_resume)(struct pci_device* device);
    int (*f_pci_suspend)(struct pci_device* device);

    pci_dev_id_t* id_table;

    /* The parent driver that makes up */
    struct drv_manifest* manifest;
} pci_driver_t;

bool is_pci_driver_unused(pci_driver_t* driver);

/* Every pci device may have one active driver where the vendor_id and dev_id are covered in the compat field of the driver */
void pci_device_attach_driver(pci_device_t* device, struct pci_driver* driver);
void pci_device_detach_driver(pci_device_t* device, struct pci_driver* driver);

int register_pci_driver(struct pci_driver* driver);
int unregister_pci_driver(struct pci_driver* driver);

extern bool __has_registered_bridges;

pci_accessmode_t get_current_addressing_mode();

struct pci_bus* get_bridge_by_index(uint32_t bridge_index);

int pci_read32(uint32_t segment, uint32_t bus, uint32_t device, uint32_t func, uint32_t reg, uint32_t* value);
int pci_read16(uint32_t segment, uint32_t bus, uint32_t device, uint32_t func, uint32_t reg, uint16_t* value);
int pci_read8(uint32_t segment, uint32_t bus, uint32_t device, uint32_t func, uint32_t reg, uint8_t* value);

int pci_write32(uint32_t segment, uint32_t bus, uint32_t device, uint32_t func, uint32_t reg, uint32_t value);
int pci_write16(uint32_t segment, uint32_t bus, uint32_t device, uint32_t func, uint32_t reg, uint16_t value);
int pci_write8(uint32_t segment, uint32_t bus, uint32_t device, uint32_t func, uint32_t reg, uint8_t value);

int pci_device_read32(pci_device_t* device, uint32_t reg, uint32_t* value);
int pci_device_read16(pci_device_t* device, uint32_t reg, uint16_t* value);
int pci_device_read8(pci_device_t* device, uint32_t reg, uint8_t* value);

int pci_device_write32(pci_device_t* device, uint32_t reg, uint32_t value);
int pci_device_write16(pci_device_t* device, uint32_t reg, uint16_t value);
int pci_device_write8(pci_device_t* device, uint32_t reg, uint8_t value);

bool test_pci_io_type1();
bool test_pci_io_type2();
// pci scanning (I would like this to be as advanced as possible and not some idiot simple thing)
// uhm what?

void pci_set_io(pci_device_address_t* address, bool value);
void pci_set_memory(pci_device_address_t* address, bool value);
void pci_set_interrupt_line(pci_device_address_t* address, bool value);
void pci_set_bus_mastering(pci_device_address_t* address, bool value);

extern struct raw_pci_impls* early_raw_pci_impls;

#endif // !__
