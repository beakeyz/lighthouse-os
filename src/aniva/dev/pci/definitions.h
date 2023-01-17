#ifndef __ANIVA_PCI_DEFINITIONS__
#define __ANIVA_PCI_DEFINITIONS__

// TODO: more
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
  IDE_C = 0x01,
  SATA_C = 0x06,
  NVMe_C = 0x08,
} MassstorageSubClasIDType_t;

typedef enum {
  PCI_TO_PCI = 0x04,
} BridgeSubClassIDType_t;

#endif // !__ANIVA_PCI_DEFINITIONS__
