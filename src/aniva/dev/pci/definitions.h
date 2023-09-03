#ifndef __ANIVA_PCI_DEFINITIONS__
#define __ANIVA_PCI_DEFINITIONS__

#include "libk/io.h"
typedef enum pci_commands {
  PCI_COMMAND_IO_SPACE = (1 << 0),
  PCI_COMMAND_MEM_SPACE = (1 << 1),
  PCI_COMMAND_BUS_MASTER = (1 << 2),
  PCI_COMMAND_SPECIAL_CYCLES = (1 << 3),
  PCI_COMMAND_MEM_WRITE_INVALIDATE_ENABLE = (1 << 4),
  PCI_COMMAND_VGA_PALLETTE_SNOOP = (1 << 5),
  PCI_COMMAND_PARITY_ERROR_RESPONSE = (1 << 6),
  PCI_COMMAND_WAIT = (1 << 7),
  PCI_COMMAND_SERR_ENABLE = (1 << 8),
  PCI_COMMAND_FAST_BTB_ENABLE =(1 << 9), /* Fast back-to-back writes */
  PCI_COMMAND_INT_DISABLE = (1 << 10), /* INTx emulation disable */
} PCI_COMMANDS;

typedef enum pci_status {
  PCI_STATUS_IMM_READY  = 0x01,
  PCI_STATUS_INTERRUPT  = 0x08,
  PCI_STATUS_CAP_LIST   = 0x10,
  PCI_STATUS_66MHZ      = 0x20,
  PCI_STATUS_UDF        = 0x40,
  PCI_STATUS_FAST_BACK  = 0x80,
  PCI_STATUS_PARITY     = 0x100,
  /* There are more, but these are not used for now */
} PCI_STATUS;

typedef enum pci_hdr_type {
  PCI_HDR_TYPE_NORMAL = 0,
  PCI_HDR_TYPE_BRIDGE = 1,
  PCI_HDR_TYPE_CARDBUS = 2,
  PCI_HDR_TYPE_MASK = 0x7f,
} PCI_HDR_TYPE;

#define PCI_BASE_ADDRESS_SPACE      0x01 /* 0 = memory, 1 = I/O */
#define PCI_BASE_ADDRESS_SPACE_IO   0x01
#define PCI_BASE_ADDRESS_SPACE_MEM  0x00
#define  PCI_BASE_ADDRESS_MEM_TYPE_MASK	0x06
#define  PCI_BASE_ADDRESS_MEM_TYPE_32	0x00	/* 32 bit address */
#define  PCI_BASE_ADDRESS_MEM_TYPE_1M	0x02	/* Below 1M [obsolete] */
#define  PCI_BASE_ADDRESS_MEM_TYPE_64	0x04	/* 64 bit address */
#define  PCI_BASE_ADDRESS_MEM_PREFETCH	0x08	/* prefetchable? */
#define  PCI_BASE_ADDRESS_MEM_MASK	(~0x0fUL)
#define  PCI_BASE_ADDRESS_IO_MASK	(~0x03UL)
/* bit 1 is reserved if address_space = 1 */

#define PCI_ROM_ADDRESS		0x30	/* Bits 31..11 are address, 10..1 reserved */
#define  PCI_ROM_ADDRESS_ENABLE	0x01
#define PCI_ROM_ADDRESS_MASK	(~0x7ffU)

// TODO: more
typedef enum {
  IO,
  IOMEM16BIT,
  IOMEM32BIT,
  IOMEM64BIT,
} BridgeAccessType_t;

typedef enum {
  NO_CODE = 0x00,
  MASS_STORAGE = 0x01,
  NETWORK_CONTROLLER = 0x02,
  DISPLAY_CONTROLLER = 0x03,
  MULTIMEDIA = 0x04,
  MEMORY_CONTROLLER = 0x05,
  BRIDGE_DEVICE = 0x06,
  SIMPLE_COMMUNICATIONS_CONTROLLER = 0x07,
  BASE_SYSTEM_PERIPHERAL = 0x08,
  INPUT_DEVICE = 0x09,
  DOCKING_STATION = 0x0A,
  PROCESSOR = 0x0B,
  SERIAL_BUS_CONTROLLER = 0x0C,
  WIRELESS_CONTROLLER = 0x0D,
  INTELLIGENT_IO_CONTROLLER = 0x0E,
  SATELLITE_COMMUNICATIONS_CONTROLLER = 0x0F,
  ENCRYPT_DECRYPT_CONTROLLER = 0x10,
  DATA_ACQUISITION_SIGNAL_PROCESSING_CONTROLLER = 0x11,
  PROCESSING_ACCELERATOR = 0x12,
  NON_ESSENTIAL_INSTRUMENTATION = 0x13,
  NO_CLASS = 0xff
} pci_class_id_t;

typedef enum pci_subclass {

  /* Storage */
  PCI_SUBCLASS_SCSI = 0x00,
  PCI_SUBCLASS_IDE = 0x01,
  PCI_SUBCLASS_FLOPPY = 0x02,
  PCI_SUBCLASS_IPI_BUS = 0x03,
  PCI_SUBCLASS_RAID_BUS = 0x04,
  PCI_SUBCLASS_ATA = 0x05,
  PCI_SUBCLASS_SATA = 0x06,
  PCI_SUBCLASS_SERIAL_ATACHED_SCSI = 0x07,
  PCI_SUBCLASS_NVM = 0x08,
  PCI_SUBCLASS_UFS = 0x09,
  PCI_SUBCLASS_MASSSTORAGE = 0x80,

  /* Network */
  PCI_SUBCLASS_ETHERNET = 0x00,

  /* Serial bus */
  PCI_SUBCLASS_SBC_FIREWIRE = 0x00,
  PCI_SUBCLASS_SBC_ACCESS_BUS = 0x01,
  PCI_SUBCLASS_SBC_SSA = 0x02,
  PCI_SUBCLASS_SBC_USB = 0x03,
  PCI_SUBLCASS_SBC_FIBRE = 0x04,
  PCI_SUBCLASS_SBC_SMBUS = 0x05,
  PCI_SUBCLASS_SBC_INFINIBAND = 0x06,
  PCI_SUBCLASS_SBC_IPMI = 0x07,
  PCI_SUBCLASS_SBC_SERCOS = 0x08,
  PCI_SUBCLASS_SBC_CANBUS = 0x09,
  PCI_SUBCLASS_SBC = 0x80,

  /* Display controller*/
  PCI_SUBCLASS_DISPLAY_VGA = 0x00,
  PCI_SUBCLASS_DISPLAY_XGA = 0x01,
  PCI_SUBCLASS_DISPLAY_3D = 0x02,
  PCI_SUBCLASS_DISPLAY_CONTROLLER = 0x80,

  /* Multimedia*/
  PCI_SUBCLASS_MULTIMEDIA_VID = 0x00,
  PCI_SUBCLASS_MULTIMEDIA_AUDIO = 0x01,
  PCI_SUBCLASS_MULTIMEDIA_TELEPHONY = 0x02,
  PCI_SUBCLASS_MULTIMEDIA_AUDIO_DEV = 0x03,
  PCI_SUBCLASS_MULTIMEDIA_CONTROLLER = 0x80,

  /* Memory */
  PCI_SUBCLASS_MEMORY_RAM = 0x00,
  PCI_SUBCLASS_MEMORY_FLASH = 0x01,
  PCI_SUBCLASS_MEMORY_CXL = 0x02,
  PCI_SUBCLASS_MEMORY_CONTROLLER = 0x80,

  /* Bridge */
  PCI_SUBCLASS_BRIDGE_HOST = 0x00,
  PCI_SUBCLASS_BRIDGE_ISA = 0x01,
  PCI_SUBCLASS_BRIDGE_EISA = 0x02,
  PCI_SUBCLASS_BRIDGE_MICRO_CHANEL = 0x03,
  PCI_SUBCLASS_BRIDGE_PCI = 0x04,
  PCI_SUBCLASS_BRIDGE_PCMCIA = 0x05,
  PCI_SUBCLASS_BRIDGE_NUBUS = 0x06,
  PCI_SUBCLASS_BRIDGE_CARDBUS = 0x07,
  PCI_SUBCLASS_BRIDGE_RACEWAY = 0x08,
  PCI_SUBCLASS_BRIDGE_PCI_2_PCI = 0x09,
  PCI_SUBCLASS_BRIDGE_INFINIBAND_2_PCI_HOST = 0x0a,
  PCI_SUBCLASS_BRIDGE = 0x80,

  /* Communications */
  PCI_SUBCLASS_COMMUNICATION_SERIAL = 0x00,
  PCI_SUBCLASS_COMMUNICATION_PARALLEL = 0x01,
  PCI_SUBCLASS_COMMUNICATION_MULTIPORT_SERIAL = 0x02,
  PCI_SUBCLASS_COMMUNICATION_MODUM = 0x03,
  PCI_SUBCLASS_COMMUNICATION_GPIB = 0x04,
  PCI_SUBCLASS_COMMUNICATION_SMARTCARD = 0x05,
  PCI_SUBCLASS_COMMUNICATION = 0x80,

  /* Input */
  PCI_SUBCLASS_INPUT_KEYBOARD = 0x00,
  PCI_SUBCLASS_INPUT_DIGITIZER_PEN = 0x01,
  PCI_SUBCLASS_INPUT_MOUSE = 0x02,
  PCI_SUBCLASS_INPUT_SCANNER = 0x03,
  PCI_SUBCLASS_INPUT_GAMEPORT = 0x04,
  PCI_SUBCLASS_INPUT = 0x80,

  /* Docking */
  PCI_SUBCLASS_DOCKING_GENERIC = 0x00,
  PCI_SUBCLASS_DOCKING = 0x80,

  /* Processor */
  PCI_SUBCLASS_PROCESSOR_386 = 0x00,
  PCI_SUBCLASS_PROCESSOR_486 = 0x01,
  PCI_SUBCLASS_PROCESSOR_PENTIUM = 0x02,
  PCI_SUBCLASS_PROCESSOR_ALPHA = 0x10,
  PCI_SUBCLASS_PROCESSOR_POWERPC = 0x20,
  PCI_SUBCLASS_PROCESSOR_MIPS = 0x30,
  PCI_SUBCLASS_PROCESSOR_COPROCESSOR = 0x40,

  /* Wireless */
  PCI_SUBCLASS_WIRELESS_IRDA = 0x00,
  PCI_SUBCLASS_WIRELESS_CONSUMER_IR = 0x01,
  PCI_SUBCLASS_WIRELESS_RF = 0x10,
  PCI_SUBCLASS_WIRELESS_BLUETOOTH = 0x11,
  PCI_SUBCLASS_WIRELESS_BROADBAND = 0x12,
  PCI_SUBCLASS_WIRELESS_802_1a = 0x20,
  PCI_SUBCLASS_WIRELESS_802_1b = 0x21,
  PCI_SUBCLASS_WIRELESS = 0x80,

  /* Intelligent */
  PCI_SUBCLASS_INTELLIGENT_I2O = 0x00,

  /* Satellite */
  PCI_SUBCLASS_SATELLITE_TV = 0x01,
  PCI_SUBCLASS_SATELLITE_AUDIO = 0x02,
  PCI_SUBCLASS_SATELLITE_VIDEO = 0x03,
  PCI_SUBCLASS_SATELLITE_DATA = 0x04,

  /* Encryption */
  PCI_SUBCLASS_ENCRYPTION_NETWORK_AND_COMPUTING = 0x00,
  PCI_SUBCLASS_ENCRYPTION_ENTERTAINMENT = 0x10,
  PCI_SUBCLASS_ENCRYPTION = 0x80,

  /* Signal processing */
  PCI_SUBCLASS_SIGNAL_DPIO = 0x00,
  PCI_SUBCLASS_SIGNAL_PERFORMANCE = 0x01,
  PCI_SUBCLASS_SIGNAL_COMMUNICATION = 0x10,
  PCI_SUBCLASS_SIGNAL_PROCESSING_MANAGEMENT = 0x20,
  PCI_SUBCLASS_SIGNAL = 0x80,

  /* Processing accelerator */
  PCI_SUBCLASS_PROCESSING_ACCELERATORS = 0x00,
  PCI_SUBCLASS_PROCESSING_SDXI = 0x01,

} PCI_SUBCLASS;

typedef enum pci_progif {
  /* IDE Storage*/
  PCI_PROGIF_IDE_ISA_COMP_MODE_ONLY = 0x00,
  PCI_PROGIF_IDE_PCI_NATIVE_MODE_ONLY = 0x05,
  PCI_PROGIF_IDE_ISA_COMP_PLUS_NATIVE_PCI = 0x0a,
  PCI_PROGIF_IDE_PCI_NATIVE_PLUS_ISA_COMP = 0x0f,
  PCI_PROGIF_IDE_ISA_COMP_PLUS_BUS_MASTERING = 0x80,
  PCI_PROGIF_IDE_PCI_NATIVE_PLUS_BUS_MASTERING = 0x85,
  PCI_PROGIF_IDE_ISA_COMP_PLUS_FULL_FEATURESET = 0x8a,
  PCI_PROGIF_IDE_PCI_NATIVE_PLUS_FULL_FEATURESET = 0x8f,
  /* ATA Storage */
  PCI_PROGIF_ATA_ADMA_SINGLE_STEPPING = 0x20,
  PCI_PROGIF_ATA_ADMA_CONTINUOUS_OP = 0x30,
  /* SATA Storage */
  PCI_PROGIF_SATA = 0x01,
  PCI_PROGIF_SSB = 0x02,

  /* NVM Storage */
  PCI_PROGIF_NVMHCI = 0x01,
  PCI_PROGIF_NVME = 0x02,

  /* USB bus */
  PCI_PROGIF_OHCI = 0x10,
  PCI_PROGIF_UHCI = 0x20,
  PCI_PROGIF_EHCI = 0x20,
  PCI_PROGIF_XHCI = 0x30,
  PCI_PROGIF_USB4 = 0x40,
  PCI_PROGIF_UNSPECEFIED = 0x80,
  PCI_PROGIF_USB_DEV = 0xfe,
} PCI_PROGIF;

#endif // !__ANIVA_PCI_DEFINITIONS__
