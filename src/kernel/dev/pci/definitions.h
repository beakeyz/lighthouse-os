#ifndef __LIGHT_PCI_DEFINITIONS__
#define __LIGHT_PCI_DEFINITIONS__

typedef enum {
  IO,
  IOMEM16BIT,
  IOMEM32BIT,
  IOMEM64BIT,
} BridgeAccessType_t;

typedef enum {
  MASSSTORAGE = 0x01,
  MULTIMEDIA = 0x04,
  BRIDGE = 0x06,
} ClassIDType_t;

typedef enum {
  PCI_TO_PCI = 0x04,
} SubClassIDType_t;

#endif // !__LIGHT_PCI_DEFINITIONS__
