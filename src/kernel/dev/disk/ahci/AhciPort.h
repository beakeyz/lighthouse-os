#ifndef __LIGHT_AHCIPORT__
#define __LIGHT_AHCIPORT__

#include "dev/disk/ahci/definitions.h"
#include <sync/spinlock.h>
#include "libk/error.h"

struct AhciDevice;

typedef struct AhciPort {
  struct AhciDevice* m_device;
  volatile HBA_port_registers_t* m_port_regs;

  spinlock_t* m_hard_lock;

  uintptr_t m_ib_page;
  uintptr_t m_cmd_list_page;
  uintptr_t m_fis_recieve_page;

  void* m_dma_buffer;
  void* m_cmd_table_buffer;

  uint32_t m_port_index;
} AhciPort_t;

AhciPort_t* make_ahci_port(struct AhciDevice* device, volatile HBA_port_registers_t* port_regs, uint32_t index);

LIGHT_STATUS initialize_port(AhciPort_t* port);

LIGHT_STATUS ahci_port_handle_int(AhciPort_t* port);

#endif // !__LIGHT_AHCIPORT__
