#ifndef __LIGHT_BRIDGE__
#define __LIGHT_BRIDGE__
#include <libk/stddef.h>

typedef struct PCI_Bridge {
  uint8_t start_bus;
  uint8_t end_bus;
  uint32_t base_addr;
  uint32_t index;
} __attribute__((packed)) PCI_Bridge_t;

void* map_bus(PCI_Bridge_t* this, uint8_t bus_num);

void write_field32(PCI_Bridge_t* this, uint8_t bus, uint8_t device, uint8_t function, uint32_t field, uint32_t value);
void write_field16(PCI_Bridge_t* this, uint8_t bus, uint8_t device, uint8_t function, uint32_t field, uint16_t value);
void write_field8(PCI_Bridge_t* this, uint8_t bus, uint8_t device, uint8_t function, uint32_t field, uint8_t value);

uint32_t read_field32(PCI_Bridge_t* this, uint8_t bus, uint8_t device, uint8_t function, uint32_t field);
uint16_t read_field16(PCI_Bridge_t* this, uint8_t bus, uint8_t device, uint8_t function, uint32_t field);
uint8_t read_field8(PCI_Bridge_t* this, uint8_t bus, uint8_t device, uint8_t function, uint32_t field);

#endif // !__LIGHT_BRIDGE__
