#include "ahci_port.h"
#include "ahci_device.h"
#include "dev/debug/serial.h"
#include "libk/error.h"
#include "libk/io.h"
#include "libk/stddef.h"
#include <mem/heap.h>
#include "mem/kmem_manager.h"
#include "sync/spinlock.h"

static ALWAYS_INLINE void port_spinup(ahci_port_t* port);
static ALWAYS_INLINE void port_power_on(ahci_port_t* port);
static ALWAYS_INLINE void port_start_clp(ahci_port_t* port);
static ALWAYS_INLINE void port_stop_clp(ahci_port_t* port);
static ALWAYS_INLINE void port_start_fis_recieving(ahci_port_t* port);
static ALWAYS_INLINE void port_stop_fis_recieving(ahci_port_t* port);
static ALWAYS_INLINE void port_set_active(ahci_port_t* port);
static ALWAYS_INLINE void port_set_sleeping(ahci_port_t* port);
static ALWAYS_INLINE ANIVA_STATUS port_sata_reset(ahci_port_t* port);
static ALWAYS_INLINE bool port_has_phy(ahci_port_t* port);

ahci_port_t* make_ahci_port(struct ahci_device* device, volatile HBA_port_registers_t* port_regs, uint32_t index) {

  uintptr_t ib_page = kmem_prepare_new_physical_page().m_ptr;

  ahci_port_t* ret = kmalloc(sizeof(ahci_port_t));
  ret->m_port_index = index;
  ret->m_device = device;
  ret->m_port_regs = port_regs;
  ret->m_ib_page = ib_page;

  ret->m_hard_lock = create_spinlock();

  // prepare buffers
  ret->m_fis_recieve_page = kmem_prepare_new_physical_page().m_ptr;
  ret->m_cmd_list_page = kmem_prepare_new_physical_page().m_ptr;

  ret->m_dma_buffer = (void*)kmem_prepare_new_physical_page().m_ptr;

  uintptr_t cmd_buffer_page = kmem_request_physical_page().m_ptr;
  ret->m_cmd_table_buffer = kmem_kernel_alloc_extended(cmd_buffer_page, SMALL_PAGE_SIZE, KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE, KMEM_FLAG_WRITABLE | KMEM_FLAG_KERNEL | KMEM_FLAG_NOCACHE | KMEM_FLAG_WRITETHROUGH);

  return ret;
}

void destroy_ahci_port(ahci_port_t* port) {

  kmem_return_physical_page(port->m_fis_recieve_page);
  kmem_return_physical_page((paddr_t)port->m_cmd_table_buffer);
  kmem_return_physical_page(port->m_cmd_list_page);
  kmem_kernel_dealloc((vaddr_t)port->m_cmd_table_buffer, SMALL_PAGE_SIZE);

  destroy_spinlock(port->m_hard_lock);
  kfree(port);
}

ANIVA_STATUS initialize_port(ahci_port_t *port) {

  port_start_fis_recieving(port);

  // clear
  port->m_port_regs->serial_ata_error = port->m_port_regs->serial_ata_error;

  if (port_sata_reset(port) == ANIVA_FAIL) {
    return ANIVA_FAIL;
  }

  if (!port_has_phy(port)) {
    return ANIVA_FAIL;
  }

  port_stop_clp(port);
  port_stop_fis_recieving(port);

  for (uint32_t i = 0;; i++) {
    if (i > 1000) {
      return ANIVA_FAIL;
    }

    if (!(port->m_port_regs->cmd_and_status & (1 << 15)) && !(port->m_port_regs->cmd_and_status & (1 << 14)))
      break;

    delay(1000);
  }

  port->m_port_regs->command_list_base_addr = (uint32_t)port->m_cmd_list_page;
  port->m_port_regs->command_list_base_addr_upper = (uint32_t)(port->m_cmd_list_page >> 32);
  port->m_port_regs->fis_base_addr = (uint32_t)port->m_fis_recieve_page;
  port->m_port_regs->fis_base_addr_upper = (uint32_t)(port->m_fis_recieve_page >> 32);
  
  port_power_on(port);
  port_spinup(port);
  port->m_port_regs->serial_ata_error = port->m_port_regs->serial_ata_error;

  port_start_fis_recieving(port);
  port_set_active(port);

  port_start_clp(port);

  return ANIVA_SUCCESS;
}

ANIVA_STATUS ahci_port_handle_int(ahci_port_t* port) {

  println("Port could handle AHCI interrupt");

  return ANIVA_FAIL;
}

// TODO: check capabilities and crap
static ALWAYS_INLINE void port_spinup(ahci_port_t* port) {

  port->m_port_regs->cmd_and_status = port->m_port_regs->cmd_and_status | (1 << 1);
}

static ALWAYS_INLINE void port_power_on(ahci_port_t* port) {

  if (!(port->m_port_regs->cmd_and_status & (1 << 20)))
    return;

  port->m_port_regs->cmd_and_status = port->m_port_regs->cmd_and_status | (1 << 2);
}

static ALWAYS_INLINE void port_start_clp(ahci_port_t* port) {

  port->m_port_regs->cmd_and_status = port->m_port_regs->cmd_and_status | 1;
}

static ALWAYS_INLINE void port_stop_clp(ahci_port_t* port) {

  port->m_port_regs->cmd_and_status = port->m_port_regs->cmd_and_status & 0xfffffffe;
}
static ALWAYS_INLINE void port_start_fis_recieving(ahci_port_t* port) {

  port->m_port_regs->cmd_and_status = port->m_port_regs->cmd_and_status | (1 << 4);
}
static ALWAYS_INLINE void port_stop_fis_recieving(ahci_port_t* port) {

  port->m_port_regs->cmd_and_status = port->m_port_regs->cmd_and_status & 0xffffffef;
}
static ALWAYS_INLINE void port_set_active(ahci_port_t* port) {
  port->m_port_regs->cmd_and_status = (port->m_port_regs->cmd_and_status & 0x0ffffff) | (1 << 28);
}
static ALWAYS_INLINE void port_set_sleeping(ahci_port_t* port) {

  port->m_port_regs->cmd_and_status = (port->m_port_regs->cmd_and_status & 0x0ffffff) | (0b1000 << 28);
}
static ALWAYS_INLINE bool port_has_phy(ahci_port_t* port) {
  return (port->m_port_regs->serial_ata_status & 0xf) == 3;
}
static ALWAYS_INLINE ANIVA_STATUS port_sata_reset(ahci_port_t* port) {
  port_stop_clp(port);

  for (uint32_t retry = 0;;retry++) {
    if (retry > 500) {
      break;
    }
    if (!(port->m_port_regs->cmd_and_status & (1 << 15)))
      break;
    delay(1000);
  }

  port_spinup(port);

  port->m_port_regs->serial_ata_control = (port->m_port_regs->serial_ata_control & 0xfffffff0) | 1;

  delay(1001);

  port->m_port_regs->serial_ata_control = (port->m_port_regs->serial_ata_control & 0xfffffff0);
  
  for (uint32_t retry = 0;;retry++) {
    if (retry > 100) {
      break;
    }
    if (port_has_phy(port))
      break;
    delay(1000);
  }

  port->m_port_regs->serial_ata_error = port->m_port_regs->serial_ata_error;

  if ((port->m_port_regs->serial_ata_status & 0xf) == 3) {
    return ANIVA_SUCCESS;
  }

  return ANIVA_FAIL;
}

