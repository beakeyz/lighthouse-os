#ifndef __ANIVA_ACPI_STRUCTURES__ 
#define __ANIVA_ACPI_STRUCTURES__
#include <libk/stddef.h>

// TODO: yoink structs from UEFI specs

typedef struct {
  char signature[8];
  uint8_t checksum;
  char OEMID[6];
  uint8_t revision;
  uint32_t rsdt_addr;
} __attribute__((packed)) RSDP_t;

typedef struct {
  RSDP_t base;
  uint32_t length;
  uintptr_t xsdt_addr;
  uint8_t extended_checksum;
  uint8_t reserved[3];
} __attribute__((packed)) XSDP_t;

typedef struct {
  char signature[4];
  uint32_t length;
  uint8_t revision;
  uint8_t checksum;
  char OEMID[6];
  char OEM_TABLE_ID[8];
  uint32_t oem_revision;
  uint32_t creator_id;
  uint32_t creator_revision;
} __attribute__((packed)) SDT_header_t;

typedef struct {
  SDT_header_t base;
  uint32_t tables[];
} __attribute__((packed)) RSDT_t;

typedef struct {
  SDT_header_t base;
  uintptr_t tables[];
} __attribute__((packed)) XSDT_t;

typedef struct {
  SDT_header_t header;
  // TODO:
} __attribute__((packed)) FADT_t;

typedef struct {
  uint64_t base_addr;
  uint16_t segment_group_number;
  uint8_t start_bus;
  uint8_t end_bus;
  uint32_t reserved;
} __attribute__((packed)) PCI_Device_Descriptor_t;

typedef struct {
  SDT_header_t header;
  uint64_t reserved;
  PCI_Device_Descriptor_t descriptors[];
} __attribute__((packed)) MCFG_t;

#endif // !