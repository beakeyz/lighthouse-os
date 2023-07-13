#include "pci.h"
#include "io.h"
#include <libk/io.h>

/*
 * type 1 pci addressing
 */
int pci_io_write32_type1(uint32_t bus, uint32_t dev, uint32_t func, uint32_t field, uint32_t value) {

  out32(PCI_PORT_ADDR, PCI_CONF1_ADDRESS(bus, dev, func, field));
  out32(PCI_PORT_VALUE, value);

  return 0;
}
int pci_io_write16_type1(uint32_t bus, uint32_t dev, uint32_t func, uint32_t field, uint16_t value) {

  out32(PCI_PORT_ADDR, PCI_CONF1_ADDRESS(bus, dev, func, field));
  out16(PCI_PORT_VALUE, value + (field & 2));

  return 0;
}
int pci_io_write8_type1(uint32_t bus, uint32_t dev, uint32_t func, uint32_t field, uint8_t value) {

  out32(PCI_PORT_ADDR, PCI_CONF1_ADDRESS(bus, dev, func, field));
  out8(PCI_PORT_VALUE, value + (field & 3));

  return 0;
}

int pci_io_read32_type1(uint32_t bus, uint32_t dev, uint32_t func, uint32_t field, uint32_t* value) {
  out32(PCI_PORT_ADDR, PCI_CONF1_ADDRESS(bus, dev, func, field));
  *value = in32(PCI_PORT_VALUE);

  return 0;
}
int pci_io_read16_type1(uint32_t bus, uint32_t dev, uint32_t func, uint32_t field, uint16_t* value) {

  out32(PCI_PORT_ADDR, PCI_CONF1_ADDRESS(bus, dev, func, field));
  *value = in16(PCI_PORT_VALUE + (field & 2));

  return 0;
}
int pci_io_read8_type1(uint32_t bus, uint32_t dev, uint32_t func, uint32_t field, uint8_t* value) {

  out32(PCI_PORT_ADDR, PCI_CONF1_ADDRESS(bus, dev, func, field));
  *value = in8(PCI_PORT_VALUE + (field & 3));

  return 0;
}

/*
 * type 2 pci addresssing
 * TODO: implement
 */
int pci_io_write32_type2(uint32_t bus, uint32_t dev, uint32_t func, uint32_t field, uint32_t value) { return 0; }
int pci_io_write16_type2(uint32_t bus, uint32_t dev, uint32_t func, uint32_t field, uint16_t value) { return 0; }
int pci_io_write8_type2(uint32_t bus, uint32_t dev, uint32_t func, uint32_t field, uint8_t value) { return 0; }

int pci_io_read32_type2(uint32_t bus, uint32_t dev, uint32_t func, uint32_t field, uint32_t* value) { return 0; }
int pci_io_read16_type2(uint32_t bus, uint32_t dev, uint32_t func, uint32_t field, uint16_t* value) { return 0; }
int pci_io_read8_type2(uint32_t bus, uint32_t dev, uint32_t func, uint32_t field, uint8_t* value) { return 0; }

const pci_device_ops_io_t g_pci_type1_impl = {
  .read8 = pci_io_read8_type1,
  .read16 = pci_io_read16_type1,
  .read32 = pci_io_read32_type1,

  .write8 = pci_io_write8_type1,
  .write16 = pci_io_write16_type1,
  .write32 = pci_io_write32_type1,
};
const pci_device_ops_io_t g_pci_type2_impl = {
  .read8 = pci_io_read8_type2,
  .read16 = pci_io_read16_type2,
  .read32 = pci_io_read32_type2,

  .write8 = pci_io_write8_type2,
  .write16 = pci_io_write16_type2,
  .write32 = pci_io_write32_type2,
};
