#ifndef __ANIVA_NVD_PCI_DEVICE__
#define __ANIVA_NVD_PCI_DEVICE__

#include "dev/pci/pci.h"
#include "dev/video/device.h"
#include "drivers/video/nvidia/device/subdev.h"
#include "drivers/video/nvidia/device/subdev/fuse/core.h"
#include "drivers/video/nvidia/device/subdev/therm/core.h"
#include "libk/io.h"
#include <libk/stddef.h>

struct nv_device;

typedef struct nv_pci_device_id {
  uint16_t device;
  uint16_t vendor;
  const char* label;
} nv_pci_device_id_t;

typedef int (*nv_subsys_entry_t) (struct nv_device* nvdev, enum NV_SUBDEV_TYPE type, void** subdev);

/* Thx, linux =) */
enum NV_CARD_TYPE {
  NV_04    = 0x04,
  NV_10    = 0x10,
  NV_11    = 0x11,
  NV_20    = 0x20,
  NV_30    = 0x30,
  NV_40    = 0x40,
  NV_50    = 0x50,
  NV_C0    = 0xc0,
  NV_E0    = 0xe0,
  GM100    = 0x110,
  GP100    = 0x130,
  GV100    = 0x140,
  TU100    = 0x160,
  GA100    = 0x170,
  AD100    = 0x190,
};

typedef struct nv_device {
  nv_pci_device_id_t id;
  pci_device_t* pdevice;

  size_t pri_size;
  void* pri;

  /* Entrypoints for our subsystems */
  nv_subsys_entry_t* subsys_entries;
  /* Storage points for our subsystems */
  nv_subdev_t* subdevices[NV_SUBDEV_COUNT];

  enum NV_CARD_TYPE card_type;
  uint32_t chipset;
  uint32_t chiprev;

  video_device_t* vdev;
} nv_device_t;

nv_device_t* create_nv_device(aniva_driver_t* driver, pci_device_t* pdev);

nv_subdev_t* nvdev_get_subdev(nv_device_t* device, enum NV_SUBDEV_TYPE type);

/* ./subdev/fuse/core.c */
extern nv_subdev_fuse_t* nvdev_get_fuse(nv_device_t* device);
/* ./subdev/therm/core.c */
extern nv_subdev_therm_t* nvdev_get_therm(nv_device_t* device);

/*
 * Nvidia mmio I/O routines
 */


/*
 * 8-bit I/O routines
 */
static inline uint8_t nvdev_rd8(nv_device_t* dev, uintptr_t offset)
{
  return mmio_read_byte(dev->pri + offset);
}
static inline void nvdev_wr8(nv_device_t* dev, uintptr_t offset, uint8_t value)
{
  return mmio_write_byte(dev->pri + offset, value);
}

/*
 * 16-bit I/O routines
 */
static inline uint16_t nvdev_rd16(nv_device_t* dev, uintptr_t offset)
{
  return mmio_read_word(dev->pri + offset);
}
static inline void nvdev_wr16(nv_device_t* dev, uintptr_t offset, uint16_t value)
{
  return mmio_write_word(dev->pri + offset, value);
}

/*
 * 32-bit I/O routines
 */
static inline uint32_t nvdev_rd32(nv_device_t* dev, uintptr_t offset)
{
  return mmio_read_dword(dev->pri + offset);
}
static inline void nvdev_wr32(nv_device_t* dev, uintptr_t offset, uint32_t value)
{
  return mmio_write_dword(dev->pri + offset, value);
}

/*
 * Mask bits
 */
static inline uint32_t nvd_mask(nv_device_t* dev, uint32_t address, uint32_t mask, uint32_t value)
{
  uint32_t _temp = nvdev_rd32(dev, address);
  nvdev_wr32(dev, address, (_temp & ~(mask)) | value);

  return _temp;
}

#endif // !__ANIVA_NVD_PCI_DEVICE__
