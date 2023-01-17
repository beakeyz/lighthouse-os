#ifndef __ANIVA_AHCI_DEFINITIONS__
#define __ANIVA_AHCI_DEFINITIONS__
#include <libk/stddef.h>

#define MAX_HBA_PORT_AMOUNT 32

typedef struct {
  uint32_t capabilities;
  uint32_t global_host_ctrl;
  uint32_t int_status;
  uint32_t ports_impl;
  uint32_t version;
  uint32_t ccc_ctrl;
  uint32_t ccc_ports;
  uint32_t eml;
  uint32_t emc;
  uint32_t capabilities_ext;
  uint32_t bios_os_hcs;
} __attribute__((packed)) HBA_host_control_t;

typedef struct {
  uint32_t command_list_base_addr;
  uint32_t command_list_base_addr_upper;
  uint32_t fis_base_addr;
  uint32_t fis_base_addr_upper;
  uint32_t int_status;
  uint32_t int_enable;
  uint32_t cmd_and_status;
  uint32_t reserved0;
  uint32_t task_file_data;
  uint32_t signature;
  uint32_t serial_ata_status;
  uint32_t serial_ata_control;
  uint32_t serial_ata_error;
  uint32_t serial_ata_active;
  uint32_t command_issue;
  uint32_t serial_ata_notif;
  uint32_t fis_switching_ctrl;
  uint32_t dev_sleep;
  uint8_t reserved1[16];
  uint8_t vs[16];
} __attribute__((packed)) HBA_port_registers_t;

typedef struct {
  HBA_host_control_t control_regs;
  uint8_t reserved0[52];
  uint8_t reserved_nvmhci[64];
  uint8_t reserved_vendor_spec[96];
  HBA_port_registers_t ports[32];
} __attribute__((packed)) HBA;

typedef struct {
  uint32_t base_low;
  uint32_t base_high;
  uint32_t reserved0;
  uint32_t byte_count;
} __attribute__((packed)) PhysRegionDesc;

typedef struct {
  uint16_t attr;
  uint16_t phys_region_desc_table_lenght;
  uint32_t phys_region_desc_table_byte_count;
  uint32_t command_table_base_addr;
  uint32_t command_table_base_addr_upper;
  uint32_t reserved0[4];
} __attribute__((packed)) CommandHeader_t;

typedef struct {
  uint8_t command_fis[64];
  uint8_t atapi_cmd[32];
  uint8_t reserved0[32];
  PhysRegionDesc descriptors[];
} __attribute__((packed)) CommandTable_t;

#endif // !__ANIVA_AHCI_DEFINITIONS__
