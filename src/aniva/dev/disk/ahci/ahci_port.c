#include "ahci_port.h"
#include "ahci_device.h"
#include "dev/debug/serial.h"
#include "dev/disk/ahci/definitions.h"
#include "libk/error.h"
#include "libk/io.h"
#include "libk/stddef.h"
#include <mem/heap.h>
#include "mem/kmem_manager.h"
#include "sync/spinlock.h"

static ALWAYS_INLINE uint32_t ahci_port_mmio_read32(ahci_port_t* port, uintptr_t offset) {
  volatile uint32_t* data = (volatile uint32_t*)(port->m_port_offset + offset);
  return *data;
}

static ALWAYS_INLINE void ahci_port_mmio_write32(ahci_port_t* port, uintptr_t offset, uint32_t data) {
  volatile uint32_t* current_data = (volatile uint32_t*)(port->m_port_offset + offset);
  *current_data = data;
}

static ALWAYS_INLINE void port_spinup(ahci_port_t* port);
static ALWAYS_INLINE void port_power_on(ahci_port_t* port);
static ALWAYS_INLINE void port_start_clp(ahci_port_t* port);
static ALWAYS_INLINE void port_stop_clp(ahci_port_t* port);
static ALWAYS_INLINE void port_start_fis_recieving(ahci_port_t* port);
static ALWAYS_INLINE void port_stop_fis_recieving(ahci_port_t* port);
static ALWAYS_INLINE void port_set_active(ahci_port_t* port);
static ALWAYS_INLINE void port_set_sleeping(ahci_port_t* port);
static ALWAYS_INLINE bool port_has_phy(ahci_port_t* port);

static ALWAYS_INLINE ANIVA_STATUS port_gather_info(ahci_port_t* port);

ahci_port_t* make_ahci_port(struct ahci_device* device, uintptr_t port_offset, uint32_t index) {

  uintptr_t ib_page = kmem_prepare_new_physical_page().m_ptr;

  ahci_port_t* ret = kmalloc(sizeof(ahci_port_t));
  ret->m_port_index = index;
  ret->m_device = device;
  ret->m_port_offset = port_offset;
  ret->m_ib_page = ib_page;

  ret->m_hard_lock = create_spinlock();

  // prepare buffers
  ret->m_fis_recieve_page = kmem_kernel_alloc_range(SMALL_PAGE_SIZE, KMEM_CUSTOMFLAG_GET_MAKE | KMEM_CUSTOMFLAG_IDENTITY, KMEM_FLAG_DMA).m_ptr;
  ret->m_cmd_list_page = kmem_kernel_alloc_range(SMALL_PAGE_SIZE, KMEM_CUSTOMFLAG_GET_MAKE | KMEM_CUSTOMFLAG_IDENTITY, KMEM_FLAG_DMA).m_ptr;

  ret->m_dma_buffer = (void*)kmem_kernel_alloc_range(SMALL_PAGE_SIZE, KMEM_CUSTOMFLAG_GET_MAKE | KMEM_CUSTOMFLAG_IDENTITY, KMEM_FLAG_DMA).m_ptr;

  ret->m_cmd_table_buffer = (void*)kmem_kernel_alloc_range(SMALL_PAGE_SIZE, KMEM_CUSTOMFLAG_IDENTITY, KMEM_FLAG_DMA).m_ptr;

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

  port_set_sleeping(port);

  uint32_t cmd_and_status = ahci_port_mmio_read32(port, AHCI_REG_PxCMD);

  // Wait for command list running
  while (cmd_and_status & AHCI_PxCMD_CR) {
    cmd_and_status = ahci_port_mmio_read32(port, AHCI_REG_PxCMD);
  }

  // Disable FIS
  port_stop_fis_recieving(port);

  // Refresh cmd_and_status
  cmd_and_status = ahci_port_mmio_read32(port, AHCI_REG_PxCMD);

  // TODO: export to function/marco?
  while (cmd_and_status & AHCI_PxCMD_FR) {
    cmd_and_status = ahci_port_mmio_read32(port, AHCI_REG_PxCMD);
  }

  uintptr_t cmd_list = port->m_cmd_list_page;
  uintptr_t fis_recieve = port->m_fis_recieve_page;

  ahci_port_mmio_write32(port, AHCI_REG_PxCLB, cmd_list & 0xFFFFFFFF);
  ahci_port_mmio_write32(port, AHCI_REG_PxCLBU, cmd_list >> 32);
  ahci_port_mmio_write32(port, AHCI_REG_PxFB, fis_recieve & 0xFFFFFFFF);
  ahci_port_mmio_write32(port, AHCI_REG_PxFBU, fis_recieve >> 32);

  // Start recieving FIS
  port_start_fis_recieving(port);

  // Reset errors
  ahci_port_mmio_write32(port, AHCI_REG_PxSERR, ahci_port_mmio_read32(port, AHCI_REG_PxSERR));

  // Reset interrupts
  ahci_port_mmio_write32(port, AHCI_REG_PxIE, 0);
  ahci_port_mmio_write32(port, AHCI_REG_PxIS, ahci_port_mmio_read32(port, AHCI_REG_PxIS));

  uint32_t tfd = ahci_port_mmio_read32(port, AHCI_REG_PxTFD);
  uint32_t sata_stat = ahci_port_mmio_read32(port, AHCI_REG_PxSSTS);

  if ((tfd & (AHCI_PxTFD_DRQ | AHCI_PxTFD_BSY)) == 0) {
    if ((sata_stat & 0x0F) == 0x03) {
      // Device connected, let's ask it for it's info

      ANIVA_STATUS info_gather_result = port_gather_info(port);

      return ANIVA_SUCCESS;
    }
  }

  port_stop_fis_recieving(port);

  return ANIVA_FAIL;
}

ANIVA_STATUS ahci_port_handle_int(ahci_port_t* port) {

  println("Port could handle AHCI interrupt");

  return ANIVA_FAIL;
}

// TODO: check capabilities and crap
static ALWAYS_INLINE void port_spinup(ahci_port_t* port) {
  uint32_t cmd_and_status = ahci_port_mmio_read32(port, AHCI_REG_PxCMD);
  cmd_and_status |= (1 << 1);
  ahci_port_mmio_write32(port, AHCI_REG_PxCMD, cmd_and_status);
}

static ALWAYS_INLINE void port_power_on(ahci_port_t* port) {
  uint32_t cmd_and_status = ahci_port_mmio_read32(port, AHCI_REG_PxCMD);

  if (!(cmd_and_status & (1 << 20)))
    return;

  cmd_and_status |= (1 << 2);
  ahci_port_mmio_write32(port, AHCI_REG_PxCMD, cmd_and_status);
}

static ALWAYS_INLINE void port_start_clp(ahci_port_t* port) {
  uint32_t cmd_and_status = ahci_port_mmio_read32(port, AHCI_REG_PxCMD);
  cmd_and_status |= (1 << 0);
  ahci_port_mmio_write32(port, AHCI_REG_PxCMD, cmd_and_status);
}

static ALWAYS_INLINE void port_stop_clp(ahci_port_t* port) {
  uint32_t cmd_and_status = ahci_port_mmio_read32(port, AHCI_REG_PxCMD);
  cmd_and_status &= 0xfffffffe;
  ahci_port_mmio_write32(port, AHCI_REG_PxCMD, cmd_and_status);
}
static ALWAYS_INLINE void port_start_fis_recieving(ahci_port_t* port) {
  uint32_t cmd_and_status = ahci_port_mmio_read32(port, AHCI_REG_PxCMD);
  cmd_and_status |= AHCI_PxCMD_FRE;
  ahci_port_mmio_write32(port, AHCI_REG_PxCMD, cmd_and_status);
}
static ALWAYS_INLINE void port_stop_fis_recieving(ahci_port_t* port) {
  uint32_t cmd_and_status = ahci_port_mmio_read32(port, AHCI_REG_PxCMD);
  cmd_and_status &= ~AHCI_PxCMD_FRE;
  ahci_port_mmio_write32(port, AHCI_REG_PxCMD, cmd_and_status);
}
static ALWAYS_INLINE void port_set_active(ahci_port_t* port) {
  uint32_t cmd_and_status = ahci_port_mmio_read32(port, AHCI_REG_PxCMD);
  cmd_and_status |= AHCI_PxCMD_ST;
  ahci_port_mmio_write32(port, AHCI_REG_PxCMD, cmd_and_status);
}
static ALWAYS_INLINE void port_set_sleeping(ahci_port_t* port) {
  uint32_t cmd_and_status = ahci_port_mmio_read32(port, AHCI_REG_PxCMD);
  cmd_and_status &= ~AHCI_PxCMD_ST;
  ahci_port_mmio_write32(port, AHCI_REG_PxCMD, cmd_and_status);
}
static ALWAYS_INLINE bool port_has_phy(ahci_port_t* port) {
  uint32_t sata_stat = ahci_port_mmio_read32(port, AHCI_REG_PxSSTS);
  return (sata_stat & 0xf) == 3;
}

static ALWAYS_INLINE ANIVA_STATUS port_gather_info(ahci_port_t* port) {

  //port_start_clp(port);

  // TODO: Bootstrap AHCI port and gather info

  return ANIVA_SUCCESS;
}

