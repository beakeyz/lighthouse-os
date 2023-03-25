#ifndef __ANIVA_AHCI_DEFINITIONS__
#define __ANIVA_AHCI_DEFINITIONS__
#include <libk/stddef.h>

#define MAX_HBA_PORT_AMOUNT 32

#define AHCI_REG_CAP 0x00 // HBA Capabilities
#define AHCI_REG_AHCI_GHC 0x04 // Global Host Control
#define AHCI_REG_IS 0x08 // Interrupt Status Register
#define AHCI_REG_PI 0x0C // Ports implemented
#define AHCI_REG_CAP2 0x24 // Host Capabilities Extended
#define AHCI_REG_AHCI_BOHC 0x28 // BIOS/OS Handoff Control and Status

#define AHCI_REG_PxCLB 0x00 // Port Command List Base Address
#define AHCI_REG_PxCLBU 0x04 // Port Command List Base Address Upper
#define AHCI_REG_PxFB 0x08 // Port FIS Base Address
#define AHCI_REG_PxFBU 0x0C // Port FIS Base Address Upper
#define AHCI_REG_PxIS 0x10 // Port Interrupt Status
#define AHCI_REG_PxIE 0x14 // Port Interrupt Enable
#define AHCI_REG_PxCMD 0x18 // Port Command and Status
#define AHCI_REG_PxTFD 0x20 // Port Task File Data
#define AHCI_REG_PxSIG 0x24 // Port Signature
#define AHCI_REG_PxSSTS 0x28 // Port Serial ATA Status
#define AHCI_REG_PxSCTL 0x2C // Port Serial ATA Control
#define AHCI_REG_PxSERR 0x30 // Port Serial ATA Error
#define AHCI_REG_PxSACT 0x34 // Port Serial ATA Active
#define AHCI_REG_PxCI 0x38 // Port Command Issue
#define AHCI_REG_PxSNTF 0x3C // Port Serial ATA Notification
#define AHCI_REG_PxFBS 0x40 // FIS-based Switching Control
#define AHCI_REG_PxDEVSLP 0x44 // Port Device Sleep
#define AHCI_REG_PxVS 0x70 // Port Vendor Specific

#define AHCI_GHC_HR (1 << 0) // HBA Reset (RW1)
#define AHCI_GHC_IE (1 << 1) // Interrupt Enable (RW)
#define AHCI_GHC_MRSM (1 << 2) // MSI Revert to Single Message (RO)
#define AHCI_GHC_AE (1U << 31) // AHCI Enable (RW/RO)

#define AHCI_CAP_S64A (1U << 31) // Supports 64-bit addressing

#define AHCI_CAP2_BOH (1 << 0) // BIOS/OS Handoff

#define AHCI_BOHC_BOS (1 << 0) // BIOS Owned Semaphore
#define AHCI_BOHC_OOS (1 << 1) // OS Owned Semaphore
#define AHCI_BOHC_BB (1 << 4) // BIOS Busy

#define AHCI_PxCMD_ST (1 << 0) // Start
#define AHCI_PxCMD_FRE (1 << 4) // FIS Receive Enable
#define AHCI_PxCMD_FR (1 << 14) // FIS Receive Running
#define AHCI_PxCMD_CR (1 << 15) // Command List Running

#define AHCI_PxTFD_DRQ (1 << 3) // Device Request
#define AHCI_PxTFD_BSY (1 << 7) // Busy

#define AHCI_PxSIG_ATA 0x101

#define AHCI_PxIE_DHRE (1 << 0) // Port Device to Host Register FIS Interrupt
#define AHCI_PxIE_PSE (1 << 1) // Port PIO Setup FIS Interrupt
#define AHCI_PxIE_DSE (1 << 2) // Port DMA Setup FIS Interrupt
#define AHCI_PxIE_SDBE (1 << 3) // Port Set Device Bits FIS Interrupt
#define AHCI_PxIE_DPE (1 << 5) // Port Descripor Processed Interrupt
#define AHCI_PORT_INTERRUPT_ERROR 0x7DC00050
#define AHCI_PORT_INTERRUPT_FULL_ENABLE 0x7DC0007F

#define AHCI_FIS_TYPE_REG_H2D 0x27
#define AHCI_FIS_TYPE_REG_D2H 0x34
#define AHCI_FIS_TYPE_DMA_ACT 0x39
#define AHCI_FIS_TYPE_DMA_SETUP 0x41


#define AHCI_COMMAND_READ_DMA_EXT 0x25
#define AHCI_COMMAND_WRITE_DMA_EXT 0x35
#define AHCI_COMMAND_FLUSH_CACHE 0xE7
#define AHCI_COMMAND_IDENTIFY_DEVICE 0xEC

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
  uint8_t type;
  uint8_t flags;
  uint8_t command;
  uint8_t features_low;
  uint8_t lba0;
  uint8_t lba1;
  uint8_t lba2;
  uint8_t dev;
  uint8_t lba3;
  uint8_t lba4;
  uint8_t lba5;
  uint8_t features_high;
  uint16_t count;
  uint8_t iso_command_complete;
  uint8_t control;
  uint32_t aux;
} __attribute__((packed)) CommandFIS_t;

typedef struct {
  CommandFIS_t fis;
  uint8_t fis_padding[44];
  uint8_t atapi_cmd[16];
  uint8_t reserved0[48];
  PhysRegionDesc descriptors;
} __attribute__((packed)) CommandTable_t;

#endif // !__ANIVA_AHCI_DEFINITIONS__
