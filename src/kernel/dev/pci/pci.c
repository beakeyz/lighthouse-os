#include "pci.h"
#include <libk/io.h>

uint32_t pci_field_read (uint32_t device_num, uint32_t field, uint32_t size) {
  
  // setup
  out32(PCI_PORT_ADDR, GET_PCI_ADDR(device_num, field));

  switch (size) {
    case 4: {
      uint32_t ret = in32(PCI_PORT_VALUE);
      return ret;
    }
    case 2: {
      uint16_t ret = in16(PCI_PORT_VALUE + (field & 2));
      return ret;
    }
    case 1: {
      uint8_t ret = in8(PCI_PORT_VALUE + (field & 3));
      return ret;
    }
    default:
      return 0xFFFF; 
  }
}

void pci_field_write (uint32_t device_num, uint32_t field, uint32_t size, uint32_t val) {
  out32(PCI_PORT_ADDR, GET_PCI_ADDR(device_num, field));
  out32(PCI_PORT_VALUE, val);
}

bool test_pci_io () {

  uint32_t tmp = 0x80000000;
  out32(PCI_PORT_ADDR, tmp);
  tmp = in32(PCI_PORT_ADDR);
  // if the value remains unchanged, we have I/O capabilities is PCI
  if (tmp == 0x80000000) {
    return true;
  }
  return false;
}

