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

typedef void* (*PCI_FUNC) (
  uint32_t dev,
  uint16_t vendor_id,
  uint16_t dev_id,
  void* stuff
);

 

#endif // !__
