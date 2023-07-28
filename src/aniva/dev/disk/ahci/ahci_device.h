#ifndef __ANIVA_AHCI_CONTROLLER__
#define __ANIVA_AHCI_CONTROLLER__
#include "dev/disk/ahci/definitions.h"
#include "dev/disk/shared.h"
#include "dev/driver.h"
#include <dev/pci/pci.h>
#include <libk/data/hive.h>

struct ahci_port;

#define AHCI_MSG_GET_PORT_COUNT 5
#define AHCI_MSG_READ 6
#define AHCI_MSG_GET_PORT 7

typedef struct ahci_driver_command_header {
 uint32_t m_crc;
 char m_port_path[32];
 void* m_req_buffer;
 size_t m_req_size;
 disk_offset_t m_req_offset;
 uint32_t m_flags;
} ahci_dch_t;

typedef struct ahci_device {
  pci_device_t* m_identifier;

  volatile HBA* m_hba_region;
  hive_t* m_ports;

  uint32_t m_used_ports;
  // TODO
} ahci_device_t;

ahci_device_t* init_ahci_device(pci_device_t* identifier);
void destroy_ahci_device(ahci_device_t* device);

ahci_dch_t* create_ahci_command_header(void* buffer, size_t size, disk_offset_t offset);
void destroy_ahci_command_header(ahci_dch_t* header);

ErrorOrPtr ahci_cmd_header_check_crc(ahci_dch_t* header);

uint32_t ahci_mmio_read32(uintptr_t base, uintptr_t offset);
void ahci_mmio_write32(uintptr_t base, uintptr_t offset, uint32_t data);

#endif // !__ANIVA_AHCI_CONTROLLER__
