#ifndef __ANIVA_ACPI_STRUCTURES__ 
#define __ANIVA_ACPI_STRUCTURES__
#include <libk/stddef.h>

#define ACPI_ES_TIMER 0x0001
#define ACPI_ES_BUS_MASTER 0x0010
#define ACPI_ES_GLOBAL 0x0020
#define ACPI_ES_PWR_BTN 0x0100
#define ACPI_ES_SLP_BTN 0x0200
#define ACPI_ES_RTC_ALARM 0x0400
#define ACPI_ES_PCIE_WAKE 0x4000
#define ACPI_ES_WAKE 0x8000

#define ACPI_GAS_MMIO 0x00
#define ACPI_GAS_IO 0x01
#define ACPI_GAS_PCI 0x02

#define ACPI_FADT_CB_ENABLED 0x0001
#define ACPI_FADT_CB_SLEEP 0x2000

#define ACPI_OPREGION_ADDR_SPACE_MEM 0x00
#define ACPI_OPREGION_ADDR_SPACE_IO 0x01
#define ACPI_OPREGION_ADDR_SPACE_PCI 0x02
#define ACPI_OPREGION_ADDR_SPACE_EC 0x03
#define ACPI_OPREGION_ADDR_SPACE_SMBUS 0x04
#define ACPI_OPREGION_ADDR_SPACE_CMOS 0x05
#define ACPI_OPREGION_ADDR_SPACE_OEM 0x80


typedef struct {
  char signature[8];
  uint8_t checksum;
  char OEMID[6];
  uint8_t revision;
  uint32_t rsdt_addr;
} __attribute__((packed)) acpi_rsdp_t;

typedef struct {
  acpi_rsdp_t base;
  uint32_t length;
  uintptr_t xsdt_addr;
  uint8_t extended_checksum;
  uint8_t reserved[3];
} __attribute__((packed)) acpi_xsdp_t;

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
} __attribute__((packed)) acpi_sdt_header_t;

typedef struct {
  acpi_sdt_header_t base;
  uint32_t tables[];
} __attribute__((packed)) acpi_rsdt_t;

typedef struct {
  acpi_sdt_header_t base;
  uintptr_t tables[];
} __attribute__((packed)) acpi_xsdt_t;

typedef struct {
  uint8_t address_space;
  uint8_t bit_width;
  uint8_t bit_offset;
  uint8_t access_size;
  uint64_t base;
} __attribute__((packed)) acpi_gas_t;

typedef struct {
  acpi_sdt_header_t header;
  uint32_t firmware_ctl;
  uint32_t dsdt_ptr;

  uint8_t reserved0;

  uint8_t profile;
  uint16_t sci_irq;
  uint32_t smi_cmd_port;
  uint8_t acpi_enable;
  uint8_t acpi_disable;
  uint8_t s4bios_req;
  uint8_t pstate_ctl;

  uint32_t pm1a_event_block;
  uint32_t pm1b_event_block;
  uint32_t pm1a_ctl_block;
  uint32_t pm1b_ctl_block;
  uint32_t pm2_ctl_block;
  uint32_t pm_timer_block;

  uint32_t gpe0_block;
  uint32_t gpe1_block;

  uint8_t pm1_event_length;
  uint8_t pm1_ctl_length;
  uint8_t pm2_ctl_length;
  uint8_t pm_timer_length;

  uint8_t gpe0_length;
  uint8_t gpe1_length;
  uint8_t gpe1_base;

  uint16_t cstate_ctl;
  uint16_t worst_c2_latency;
  uint16_t worst_c3_latency;
  uint16_t flush_size;
  uint16_t flush_stride;

  uint8_t duty_offset;
  uint8_t duty_width;

  uint8_t cmos_day_alarm;
  uint8_t cmos_month_alarm;
  uint8_t cmos_century;

  uint16_t iapc_boot_flags;
  uint8_t reserved1;
  uint32_t flags;

  acpi_gas_t reset_reg;
  uint8_t reset_cmd;
  uint16_t arm_boot_flags;
  uint8_t minor_version;

  uint64_t x_firmware_ctl;
  uint64_t x_dsdt;

  acpi_gas_t x_pm1a_event_block;
  acpi_gas_t x_pm1b_event_block;
  acpi_gas_t x_pm1a_ctl_block;
  acpi_gas_t x_pm1b_ctl_block;
  acpi_gas_t x_pm2_ctl_block;
  acpi_gas_t x_pm_timer_block;
  acpi_gas_t x_gpe0_block;
  acpi_gas_t x_gpe1_block;
  acpi_gas_t x_sleep_ctl_reg;
  acpi_gas_t x_sleep_status_reg;
} __attribute__((packed)) acpi_fadt_t;

typedef struct {
  uint64_t base_addr;
  uint16_t segment_group_number;
  uint8_t start_bus;
  uint8_t end_bus;
  uint32_t reserved;
} __attribute__((packed)) PCI_Device_Descriptor_t;

typedef struct {
  acpi_sdt_header_t header;
  uint64_t reserved;
  PCI_Device_Descriptor_t descriptors[];
} __attribute__((packed)) acpi_mcfg_t;

typedef struct {
  acpi_sdt_header_t header;
  acpi_gas_t ec_ctl;
  acpi_gas_t ec_data;
  uint32_t uid;
  uint8_t gpe_bit;
  uint8_t ec_id[];
} __attribute__((packed)) acpi_ecdt_t;

typedef struct {
  acpi_sdt_header_t header;
  uint8_t data[];
} __attribute__((packed)) acpi_aml_table_t;

typedef uint8_t acpi_resource_type_t;

#define ACPI_RESOURCE_MEM 0x01
#define ACPI_RESOURCE_IO 0x02
#define ACPI_RESOURCE_IRQ 0x03

typedef struct acpi_resource {
  acpi_resource_type_t type;
  size_t base;
  size_t length;

  uint8_t irq_flags;

  uint8_t addr_space;
  uint8_t bit_width;
  uint8_t bit_offset;
} acpi_resource_t;

#endif // !
