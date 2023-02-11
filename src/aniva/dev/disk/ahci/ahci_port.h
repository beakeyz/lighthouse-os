#ifndef __ANIVA_AHCIPORT__
#define __ANIVA_AHCIPORT__

#include "dev/disk/ahci/definitions.h"
#include <sync/spinlock.h>
#include "libk/error.h"

struct ahci_device;

typedef struct ahci_port {
  struct ahci_device* m_device;
  volatile HBA_port_registers_t* m_port_regs;

  spinlock_t* m_hard_lock;

  uintptr_t m_ib_page;
  uintptr_t m_cmd_list_page;
  uintptr_t m_fis_recieve_page;

  void* m_dma_buffer;
  void* m_cmd_table_buffer;

  uint32_t m_port_index;
} ahci_port_t;

ahci_port_t* make_ahci_port(struct ahci_device* device, volatile HBA_port_registers_t* port_regs, uint32_t index);

ANIVA_STATUS initialize_port(ahci_port_t* port);

ANIVA_STATUS ahci_port_handle_int(ahci_port_t* port);

ErrorOrPtr ahci_port_find_unused_command_header(ahci_port_t* port);

#endif // !__ANIVA_AHCIPORT__
