#ifndef __ANIVA_AHCI_CONTROLLER__
#define __ANIVA_AHCI_CONTROLLER__
#include "dev/disk/ahci/definitions.h"
#include "dev/disk/shared.h"
#include "dev/driver.h"
#include <dev/pci/pci.h>

struct ahci_port;

#define AHCI_MSG_GET_PORT_COUNT 5
#define AHCI_MSG_READ 6

typedef struct ahci_driver_command_header {
 uint32_t m_crc;
 uint32_t m_port_index;
 void* m_req_buffer;
 size_t m_req_size;
 disk_offset_t m_req_offset;
 uint32_t m_flags;
} ahci_dch_t;

typedef struct ahci_device {
  pci_device_identifier_t* m_identifier;

  volatile HBA* m_hba_region;
  list_t* m_ports;

  uint32_t m_used_ports;
  // TODO
} ahci_device_t;

ahci_device_t* init_ahci_device(pci_device_identifier_t* identifier);
void destroy_ahci_device(ahci_device_t* device);

ahci_dch_t* create_ahci_command_header(size_t size, disk_offset_t offset);
void destroy_ahci_command_header(ahci_dch_t* header);

ErrorOrPtr ahci_cmd_header_check_crc(ahci_dch_t* header);

uint32_t ahci_mmio_read32(uintptr_t base, uintptr_t offset);
void ahci_mmio_write32(uintptr_t base, uintptr_t offset, uint32_t data);

const extern aniva_driver_t g_base_ahci_driver;

#endif // !__ANIVA_AHCI_CONTROLLER__
