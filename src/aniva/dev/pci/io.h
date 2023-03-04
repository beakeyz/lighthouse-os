#ifndef __ANIVA_PCI_IO__
#define __ANIVA_PCI_IO__
#include "pci.h"

typedef struct raw_pci_impls {
  int (*write32)(uint32_t bus, uint32_t dev, uint32_t func, uint32_t field, uint32_t value);
  int (*write16)(uint32_t bus, uint32_t dev, uint32_t func, uint32_t field, uint16_t value);
  int (*write8)(uint32_t bus, uint32_t dev, uint32_t func, uint32_t field, uint8_t value);
  int (*read32)(uint32_t bus, uint32_t dev, uint32_t func, uint32_t field, uint32_t* value);
  int (*read16)(uint32_t bus, uint32_t dev, uint32_t func, uint32_t field, uint16_t* value);
  int (*read8)(uint32_t bus, uint32_t dev, uint32_t func, uint32_t field, uint8_t* value);
} raw_pci_impls_t;

/*
 * type 1 pci addressing
 */
int pci_io_write32_type1(uint32_t bus, uint32_t dev, uint32_t func, uint32_t field, uint32_t value);
int pci_io_write16_type1(uint32_t bus, uint32_t dev, uint32_t func, uint32_t field, uint16_t value);
int pci_io_write8_type1(uint32_t bus, uint32_t dev, uint32_t func, uint32_t field, uint8_t value);

int pci_io_read32_type1(uint32_t bus, uint32_t dev, uint32_t func, uint32_t field, uint32_t* value);
int pci_io_read16_type1(uint32_t bus, uint32_t dev, uint32_t func, uint32_t field, uint16_t* value);
int pci_io_read8_type1(uint32_t bus, uint32_t dev, uint32_t func, uint32_t field, uint8_t* value);

/*
 * type 2 pci addresssing
 */
int pci_io_write32_type2(uint32_t bus, uint32_t dev, uint32_t func, uint32_t field, uint32_t* value);
int pci_io_write16_type2(uint32_t bus, uint32_t dev, uint32_t func, uint32_t field, uint16_t* value);
int pci_io_write8_type2(uint32_t bus, uint32_t dev, uint32_t func, uint32_t field, uint8_t* value);

int pci_io_read32_type2(uint32_t bus, uint32_t dev, uint32_t func, uint32_t field, uint32_t* value);
int pci_io_read16_type2(uint32_t bus, uint32_t dev, uint32_t func, uint32_t field, uint16_t* value);
int pci_io_read8_type2(uint32_t bus, uint32_t dev, uint32_t func, uint32_t field, uint8_t* value);

extern const raw_pci_impls_t raw_pci_type1_impl;
extern const raw_pci_impls_t raw_pci_type2_impl;

#endif //__ANIVA_PCI_IO__