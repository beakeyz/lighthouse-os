#ifndef __LIGHT_PCI__
#define __LIGHT_PCI__
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

typedef void* (*PCI_FUNC) (
  uint32_t dev,
  uint16_t vendor_id,
  uint16_t dev_id,
  void* stuff
);

uint32_t pci_field_read (uint32_t device_num, uint32_t field, uint32_t size);
void pci_field_write (uint32_t device_num, uint32_t field, uint32_t size, uint32_t val);

// pci scanning (I would like this to be as advanced as possible and not some idiot simple thing)

#endif // !__
