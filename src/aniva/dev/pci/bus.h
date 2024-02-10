#ifndef __ANIVA_BRIDGE__
#define __ANIVA_BRIDGE__
#include <libk/stddef.h>

struct device;

typedef struct pci_bus {
  struct pci_bus* m_parent;
  struct device *dev;

  uint8_t start_bus;
  uint8_t end_bus;
  uint32_t base_addr;
  uint32_t index;

  bool is_mapped;
  void* mapped_base;
} pci_bus_t;

void init_pci_bus();

pci_bus_t* create_pci_bus(uint32_t base, uint8_t start, uint8_t end, uint32_t busnum, pci_bus_t* parent);
void destroy_pci_bus(pci_bus_t* bus);

void* map_bus(pci_bus_t* this, uint8_t bus_num);

void write_field32(pci_bus_t* this, uint8_t bus, uint8_t device, uint8_t function, uint32_t field, uint32_t value);
void write_field16(pci_bus_t* this, uint8_t bus, uint8_t device, uint8_t function, uint32_t field, uint16_t value);
void write_field8(pci_bus_t* this, uint8_t bus, uint8_t device, uint8_t function, uint32_t field, uint8_t value);

uint32_t read_field32(pci_bus_t* this, uint8_t bus, uint8_t device, uint8_t function, uint32_t field);
uint16_t read_field16(pci_bus_t* this, uint8_t bus, uint8_t device, uint8_t function, uint32_t field);
uint8_t read_field8(pci_bus_t* this, uint8_t bus, uint8_t device, uint8_t function, uint32_t field);

static ALWAYS_INLINE bool is_pci_bus_root(pci_bus_t* bus) 
{
  return !(bus->m_parent);
}
#endif // !__ANIVA_BRIDGE__
