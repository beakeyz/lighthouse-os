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

/*
 * We use this only for MADT for now. MADT should only have one subtable, right after 
 * the main table.
 */
typedef struct acpi_subtable_header {
  uint8_t type;
  uint8_t length;
} acpi_subtable_header_t;

/*
 * MADT
 * -> Thx, linux =)
 */

struct acpi_table_madt {
	acpi_sdt_header_t header;	/* Common ACPI table header */
	uint32_t address;		/* Physical address of local APIC */
	uint32_t flags;
};

/* Masks for Flags field above */

#define ACPI_MADT_PCAT_COMPAT       (1)	/* 00: System also has dual 8259s */

/* Values for PCATCompat flag */

#define ACPI_MADT_DUAL_PIC          1
#define ACPI_MADT_MULTIPLE_APIC     0

/* Values for MADT subtable type in struct acpi_subtable_header */

enum acpi_madt_type {
	ACPI_MADT_TYPE_LOCAL_APIC = 0,
	ACPI_MADT_TYPE_IO_APIC = 1,
	ACPI_MADT_TYPE_INTERRUPT_OVERRIDE = 2,
	ACPI_MADT_TYPE_NMI_SOURCE = 3,
	ACPI_MADT_TYPE_LOCAL_APIC_NMI = 4,
	ACPI_MADT_TYPE_LOCAL_APIC_OVERRIDE = 5,
	ACPI_MADT_TYPE_IO_SAPIC = 6,
	ACPI_MADT_TYPE_LOCAL_SAPIC = 7,
	ACPI_MADT_TYPE_INTERRUPT_SOURCE = 8,
	ACPI_MADT_TYPE_LOCAL_X2APIC = 9,
	ACPI_MADT_TYPE_LOCAL_X2APIC_NMI = 10,
	ACPI_MADT_TYPE_GENERIC_INTERRUPT = 11,
	ACPI_MADT_TYPE_GENERIC_DISTRIBUTOR = 12,
	ACPI_MADT_TYPE_GENERIC_MSI_FRAME = 13,
	ACPI_MADT_TYPE_GENERIC_REDISTRIBUTOR = 14,
	ACPI_MADT_TYPE_GENERIC_TRANSLATOR = 15,
	ACPI_MADT_TYPE_MULTIPROC_WAKEUP = 16,
	ACPI_MADT_TYPE_CORE_PIC = 17,
	ACPI_MADT_TYPE_LIO_PIC = 18,
	ACPI_MADT_TYPE_HT_PIC = 19,
	ACPI_MADT_TYPE_EIO_PIC = 20,
	ACPI_MADT_TYPE_MSI_PIC = 21,
	ACPI_MADT_TYPE_BIO_PIC = 22,
	ACPI_MADT_TYPE_LPC_PIC = 23,
	ACPI_MADT_TYPE_RINTC = 24,
	ACPI_MADT_TYPE_IMSIC = 25,
	ACPI_MADT_TYPE_APLIC = 26,
	ACPI_MADT_TYPE_PLIC = 27,
	ACPI_MADT_TYPE_RESERVED = 28,	/* 28 to 0x7F are reserved */
	ACPI_MADT_TYPE_OEM_RESERVED = 0x80	/* 0x80 to 0xFF are reserved for OEM use */
};

/*
 * MADT Subtables, correspond to Type in struct acpi_subtable_header
 */

/* 0: Processor Local APIC */

struct acpi_madt_local_apic {
	struct acpi_subtable_header header;
	uint8_t processor_id;	/* ACPI processor id */
	uint8_t id;			/* Processor's local APIC id */
	uint32_t lapic_flags;
};

/* 1: IO APIC */

struct acpi_madt_io_apic {
	struct acpi_subtable_header header;
	uint8_t id;			/* I/O APIC ID */
	uint8_t reserved;		/* reserved - must be zero */
	uint32_t address;		/* APIC physical address */
	uint32_t global_irq_base;	/* Global system interrupt where INTI lines start */
};

/* 2: Interrupt Override */

struct acpi_madt_interrupt_override {
	struct acpi_subtable_header header;
	uint8_t bus;			/* 0 - ISA */
	uint8_t source_irq;		/* Interrupt source (IRQ) */
	uint32_t global_irq;		/* Global system interrupt */
	uint16_t inti_flags;
};

/* 3: NMI Source */

struct acpi_madt_nmi_source {
	struct acpi_subtable_header header;
	uint16_t inti_flags;
	uint32_t global_irq;		/* Global system interrupt */
};

/* 4: Local APIC NMI */

struct acpi_madt_local_apic_nmi {
	struct acpi_subtable_header header;
	uint8_t processor_id;	/* ACPI processor id */
	uint16_t inti_flags;
	uint8_t lint;		/* LINTn to which NMI is connected */
};

/* 5: Address Override */

struct acpi_madt_local_apic_override {
	struct acpi_subtable_header header;
	uint16_t reserved;		/* Reserved, must be zero */
	uint64_t address;		/* APIC physical address */
};

/* 6: I/O Sapic */

struct acpi_madt_io_sapic {
	struct acpi_subtable_header header;
	uint8_t id;			/* I/O SAPIC ID */
	uint8_t reserved;		/* Reserved, must be zero */
	uint32_t global_irq_base;	/* Global interrupt for SAPIC start */
	uint64_t address;		/* SAPIC physical address */
};

/* 7: Local Sapic */

struct acpi_madt_local_sapic {
	struct acpi_subtable_header header;
	uint8_t processor_id;	/* ACPI processor id */
	uint8_t id;			/* SAPIC ID */
	uint8_t eid;			/* SAPIC EID */
	uint8_t reserved[3];		/* Reserved, must be zero */
	uint32_t lapic_flags;
	uint32_t uid;		/* Numeric UID - ACPI 3.0 */
	char uid_string[];	/* String UID  - ACPI 3.0 */
};

/* 8: Platform Interrupt Source */

struct acpi_madt_interrupt_source {
	struct acpi_subtable_header header;
	uint16_t inti_flags;
	uint8_t type;		/* 1=PMI, 2=INIT, 3=corrected */
	uint8_t id;			/* Processor ID */
	uint8_t eid;			/* Processor EID */
	uint8_t io_sapic_vector;	/* Vector value for PMI interrupts */
	uint32_t global_irq;		/* Global system interrupt */
	uint32_t flags;		/* Interrupt Source Flags */
};

/* Masks for Flags field above */

#define ACPI_MADT_CPEI_OVERRIDE     (1)

/* 9: Processor Local X2APIC (ACPI 4.0) */

struct acpi_madt_local_x2apic {
	struct acpi_subtable_header header;
	uint16_t reserved;		/* reserved - must be zero */
	uint32_t local_apic_id;	/* Processor x2APIC ID  */
	uint32_t lapic_flags;
	uint32_t uid;		/* ACPI processor UID */
};

/* 10: Local X2APIC NMI (ACPI 4.0) */

struct acpi_madt_local_x2apic_nmi {
	struct acpi_subtable_header header;
	uint16_t inti_flags;
	uint32_t uid;		/* ACPI processor UID */
	uint8_t lint;		/* LINTn to which NMI is connected */
	uint8_t reserved[3];		/* reserved - must be zero */
};

/* 11: Generic interrupt - GICC (ACPI 5.0 + ACPI 6.0 + ACPI 6.3 + ACPI 6.5 changes) */

struct acpi_madt_generic_interrupt {
	struct acpi_subtable_header header;
	uint16_t reserved;		/* reserved - must be zero */
	uint32_t cpu_interface_number;
	uint32_t uid;
	uint32_t flags;
	uint32_t parking_version;
	uint32_t performance_interrupt;
	uint64_t parked_address;
	uint64_t base_address;
	uint64_t gicv_base_address;
	uint64_t gich_base_address;
	uint32_t vgic_interrupt;
	uint64_t gicr_base_address;
	uint64_t arm_mpidr;
	uint8_t efficiency_class;
	uint8_t reserved2[1];
	uint16_t spe_interrupt;	/* ACPI 6.3 */
	uint16_t trbe_interrupt;	/* ACPI 6.5 */
};

/* Masks for Flags field above */

/* ACPI_MADT_ENABLED                    (1)      Processor is usable if set */
#define ACPI_MADT_PERFORMANCE_IRQ_MODE  (1<<1)	/* 01: Performance Interrupt Mode */
#define ACPI_MADT_VGIC_IRQ_MODE         (1<<2)	/* 02: VGIC Maintenance Interrupt mode */

/* 12: Generic Distributor (ACPI 5.0 + ACPI 6.0 changes) */

struct acpi_madt_generic_distributor {
	struct acpi_subtable_header header;
	uint16_t reserved;		/* reserved - must be zero */
	uint32_t gic_id;
	uint64_t base_address;
	uint32_t global_irq_base;
	uint8_t version;
	uint8_t reserved2[3];	/* reserved - must be zero */
};

/* Values for Version field above */

enum acpi_madt_gic_version {
	ACPI_MADT_GIC_VERSION_NONE = 0,
	ACPI_MADT_GIC_VERSION_V1 = 1,
	ACPI_MADT_GIC_VERSION_V2 = 2,
	ACPI_MADT_GIC_VERSION_V3 = 3,
	ACPI_MADT_GIC_VERSION_V4 = 4,
	ACPI_MADT_GIC_VERSION_RESERVED = 5	/* 5 and greater are reserved */
};

/* 13: Generic MSI Frame (ACPI 5.1) */

struct acpi_madt_generic_msi_frame {
	struct acpi_subtable_header header;
	uint16_t reserved;		/* reserved - must be zero */
	uint32_t msi_frame_id;
	uint64_t base_address;
	uint32_t flags;
	uint16_t spi_count;
	uint16_t spi_base;
};

/* Masks for Flags field above */

#define ACPI_MADT_OVERRIDE_SPI_VALUES   (1)

/* 14: Generic Redistributor (ACPI 5.1) */

struct acpi_madt_generic_redistributor {
	struct acpi_subtable_header header;
	uint16_t reserved;		/* reserved - must be zero */
	uint64_t base_address;
	uint32_t length;
};

/* 15: Generic Translator (ACPI 6.0) */

struct acpi_madt_generic_translator {
	struct acpi_subtable_header header;
	uint16_t reserved;		/* reserved - must be zero */
	uint32_t translation_id;
	uint64_t base_address;
	uint32_t reserved2;
};

/* 16: Multiprocessor wakeup (ACPI 6.4) */

struct acpi_madt_multiproc_wakeup {
	struct acpi_subtable_header header;
	uint16_t mailbox_version;
	uint32_t reserved;		/* reserved - must be zero */
	uint64_t base_address;
};

#define ACPI_MULTIPROC_WAKEUP_MB_OS_SIZE        2032
#define ACPI_MULTIPROC_WAKEUP_MB_FIRMWARE_SIZE  2048

struct acpi_madt_multiproc_wakeup_mailbox {
	uint16_t command;
	uint16_t reserved;		/* reserved - must be zero */
	uint32_t apic_id;
	uint64_t wakeup_vector;
	uint8_t reserved_os[ACPI_MULTIPROC_WAKEUP_MB_OS_SIZE];	/* reserved for OS use */
	uint8_t reserved_firmware[ACPI_MULTIPROC_WAKEUP_MB_FIRMWARE_SIZE];	/* reserved for firmware use */
};

#define ACPI_MP_WAKE_COMMAND_WAKEUP    1

/* 17: CPU Core Interrupt Controller (ACPI 6.5) */

struct acpi_madt_core_pic {
	struct acpi_subtable_header header;
	uint8_t version;
	uint32_t processor_id;
	uint32_t core_id;
	uint32_t flags;
};

/* Values for Version field above */

enum acpi_madt_core_pic_version {
	ACPI_MADT_CORE_PIC_VERSION_NONE = 0,
	ACPI_MADT_CORE_PIC_VERSION_V1 = 1,
	ACPI_MADT_CORE_PIC_VERSION_RESERVED = 2	/* 2 and greater are reserved */
};

/* 18: Legacy I/O Interrupt Controller (ACPI 6.5) */

struct acpi_madt_lio_pic {
	struct acpi_subtable_header header;
	uint8_t version;
	uint64_t address;
	uint16_t size;
	uint8_t cascade[2];
	uint32_t cascade_map[2];
};

/* Values for Version field above */

enum acpi_madt_lio_pic_version {
	ACPI_MADT_LIO_PIC_VERSION_NONE = 0,
	ACPI_MADT_LIO_PIC_VERSION_V1 = 1,
	ACPI_MADT_LIO_PIC_VERSION_RESERVED = 2	/* 2 and greater are reserved */
};

/* 19: HT Interrupt Controller (ACPI 6.5) */

struct acpi_madt_ht_pic {
	struct acpi_subtable_header header;
	uint8_t version;
	uint64_t address;
	uint16_t size;
	uint8_t cascade[8];
};

/* Values for Version field above */

enum acpi_madt_ht_pic_version {
	ACPI_MADT_HT_PIC_VERSION_NONE = 0,
	ACPI_MADT_HT_PIC_VERSION_V1 = 1,
	ACPI_MADT_HT_PIC_VERSION_RESERVED = 2	/* 2 and greater are reserved */
};

/* 20: Extend I/O Interrupt Controller (ACPI 6.5) */

struct acpi_madt_eio_pic {
	struct acpi_subtable_header header;
	uint8_t version;
	uint8_t cascade;
	uint8_t node;
	uint64_t node_map;
};

/* Values for Version field above */

enum acpi_madt_eio_pic_version {
	ACPI_MADT_EIO_PIC_VERSION_NONE = 0,
	ACPI_MADT_EIO_PIC_VERSION_V1 = 1,
	ACPI_MADT_EIO_PIC_VERSION_RESERVED = 2	/* 2 and greater are reserved */
};

/* 21: MSI Interrupt Controller (ACPI 6.5) */

struct acpi_madt_msi_pic {
	struct acpi_subtable_header header;
	uint8_t version;
	uint64_t msg_address;
	uint32_t start;
	uint32_t count;
};

/* Values for Version field above */

enum acpi_madt_msi_pic_version {
	ACPI_MADT_MSI_PIC_VERSION_NONE = 0,
	ACPI_MADT_MSI_PIC_VERSION_V1 = 1,
	ACPI_MADT_MSI_PIC_VERSION_RESERVED = 2	/* 2 and greater are reserved */
};

/* 22: Bridge I/O Interrupt Controller (ACPI 6.5) */

struct acpi_madt_bio_pic {
	struct acpi_subtable_header header;
	uint8_t version;
	uint64_t address;
	uint16_t size;
	uint16_t id;
	uint16_t gsi_base;
};

/* Values for Version field above */

enum acpi_madt_bio_pic_version {
	ACPI_MADT_BIO_PIC_VERSION_NONE = 0,
	ACPI_MADT_BIO_PIC_VERSION_V1 = 1,
	ACPI_MADT_BIO_PIC_VERSION_RESERVED = 2	/* 2 and greater are reserved */
};

/* 23: LPC Interrupt Controller (ACPI 6.5) */

struct acpi_madt_lpc_pic {
	struct acpi_subtable_header header;
	uint8_t version;
	uint64_t address;
	uint16_t size;
	uint8_t cascade;
};

/* Values for Version field above */

enum acpi_madt_lpc_pic_version {
	ACPI_MADT_LPC_PIC_VERSION_NONE = 0,
	ACPI_MADT_LPC_PIC_VERSION_V1 = 1,
	ACPI_MADT_LPC_PIC_VERSION_RESERVED = 2	/* 2 and greater are reserved */
};

/* 24: RISC-V INTC */
struct acpi_madt_rintc {
	struct acpi_subtable_header header;
	uint8_t version;
	uint8_t reserved;
	uint32_t flags;
	uint64_t hart_id;
	uint32_t uid;		/* ACPI processor UID */
	uint32_t ext_intc_id;	/* External INTC Id */
	uint64_t imsic_addr;		/* IMSIC base address */
	uint32_t imsic_size;		/* IMSIC size */
};

/* Values for RISC-V INTC Version field above */

enum acpi_madt_rintc_version {
	ACPI_MADT_RINTC_VERSION_NONE = 0,
	ACPI_MADT_RINTC_VERSION_V1 = 1,
	ACPI_MADT_RINTC_VERSION_RESERVED = 2	/* 2 and greater are reserved */
};

/* 25: RISC-V IMSIC */
struct acpi_madt_imsic {
	struct acpi_subtable_header header;
	uint8_t version;
	uint8_t reserved;
	uint32_t flags;
	uint16_t num_ids;
	uint16_t num_guest_ids;
	uint8_t guest_index_bits;
	uint8_t hart_index_bits;
	uint8_t group_index_bits;
	uint8_t group_index_shift;
};

/* 26: RISC-V APLIC */
struct acpi_madt_aplic {
	struct acpi_subtable_header header;
	uint8_t version;
	uint8_t id;
	uint32_t flags;
	uint8_t hw_id[8];
	uint16_t num_idcs;
	uint16_t num_sources;
	uint32_t gsi_base;
	uint64_t base_addr;
	uint32_t size;
};

/* 27: RISC-V PLIC */
struct acpi_madt_plic {
	struct acpi_subtable_header header;
	uint8_t version;
	uint8_t id;
	uint8_t hw_id[8];
	uint16_t num_irqs;
	uint16_t max_prio;
	uint32_t flags;
	uint32_t size;
	uint64_t base_addr;
	uint32_t gsi_base;
};

/*
 * Common flags fields for MADT subtables
 */

/* MADT Local APIC flags */

#define ACPI_MADT_ENABLED           (1)	/* 00: Processor is usable if set */
#define ACPI_MADT_ONLINE_CAPABLE    (2)	/* 01: System HW supports enabling processor at runtime */

/* MADT MPS INTI flags (inti_flags) */

#define ACPI_MADT_POLARITY_MASK     (3)	/* 00-01: Polarity of APIC I/O input signals */
#define ACPI_MADT_TRIGGER_MASK      (3<<2)	/* 02-03: Trigger mode of APIC input signals */

/* Values for MPS INTI flags */

#define ACPI_MADT_POLARITY_CONFORMS       0
#define ACPI_MADT_POLARITY_ACTIVE_HIGH    1
#define ACPI_MADT_POLARITY_RESERVED       2
#define ACPI_MADT_POLARITY_ACTIVE_LOW     3

#define ACPI_MADT_TRIGGER_CONFORMS        (0)
#define ACPI_MADT_TRIGGER_EDGE            (1<<2)
#define ACPI_MADT_TRIGGER_RESERVED        (2<<2)
#define ACPI_MADT_TRIGGER_LEVEL           (3<<2)

#endif // !
