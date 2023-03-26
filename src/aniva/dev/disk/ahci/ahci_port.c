#include "ahci_port.h"
#include "ahci_device.h"
#include "dev/debug/serial.h"
#include "dev/disk/ahci/definitions.h"
#include "dev/disk/shared.h"
#include "dev/kterm/kterm.h"
#include "libk/error.h"
#include "libk/io.h"
#include "libk/stddef.h"
#include <mem/heap.h>
#include "libk/string.h"
#include "mem/kmem_manager.h"
#include "sched/scheduler.h"
#include "sync/spinlock.h"

static ALWAYS_INLINE uint32_t ahci_port_mmio_read32(ahci_port_t* port, uintptr_t offset) {
  volatile uint32_t* data = (volatile uint32_t*)(port->m_port_offset + offset);
  return *data;
}

static ALWAYS_INLINE void ahci_port_mmio_write32(ahci_port_t* port, uintptr_t offset, uint32_t data) {
  volatile uint32_t* current_data = (volatile uint32_t*)(port->m_port_offset + offset);
  *current_data = data;
}

static ALWAYS_INLINE uint32_t ahci_port_find_free_cmd_header(ahci_port_t* port) {
  uint32_t ci = ahci_port_mmio_read32(port, AHCI_REG_PxCI);
  for (uint32_t idx = 0; idx < 32; idx++) {
    if ((ci & 1) == 0) {
      return idx;
    }
    ci >>= 1;
  }
  return (uint32_t)-1;
}

static ALWAYS_INLINE ANIVA_STATUS ahci_port_await_dma_completion(ahci_port_t* port) {
  if (port->m_is_waiting) {
    
    while (port->m_awaiting_dma_transfer_complete) {
      scheduler_yield();
    }

    // Reset
    port->m_is_waiting = false;
    if (port->m_transfer_failed) {
      port->m_transfer_failed = false;
      // TODO: real dbg msg
      println("AHCIPORT: dma transfer failed!");
      return false;
    }
  }
  return ANIVA_SUCCESS;
}

static ALWAYS_INLINE ANIVA_STATUS ahci_port_await_dma_completion_sync(ahci_port_t* port) {

  if (port->m_is_waiting) {

    uintptr_t tmt;

    tmt = 0;

    // Wait until CI resets
    while (ahci_port_mmio_read32(port, AHCI_REG_PxCI)) {
      tmt++;

      // Max. 1 second
      if (tmt >= 1000) {
        goto timeout;
      }

      delay(1000);
    }

    return ANIVA_SUCCESS;
  }

timeout:
  return ANIVA_FAIL;
}

static ANIVA_STATUS ahci_port_send_command(ahci_port_t* port, uint8_t cmd, paddr_t buffer, size_t size, bool write, uintptr_t lba, uint16_t blk_count) {

  if (ahci_port_await_dma_completion(port) == false)
    return ANIVA_FAIL;

  uint32_t cmd_header_idx = ahci_port_find_free_cmd_header(port);

  ASSERT_MSG(cmd_header_idx >= 0 && cmd_header_idx <= 32, "Invalid Command AHCI header!");

  CommandHeader_t* cmd_header = &((CommandHeader_t*)port->m_cmd_list_page)[cmd_header_idx];
  CommandTable_t* cmd_table = &((CommandTable_t*)port->m_cmd_table_buffer)[cmd_header_idx];

  cmd_header->command_table_base_addr = (uintptr_t)(kmem_to_phys(nullptr, (vaddr_t)cmd_table)) & 0xFFFFFFFFUL;
  cmd_header->command_table_base_addr_upper = 0;
  cmd_header->phys_region_desc_table_lenght = size ? 1 : 0;
  cmd_header->phys_region_desc_table_byte_count = 0;

  cmd_header->attr = 5 | (1 << 7);

  /*
  if (atapi) {
    cmd_header->attr |= (1 << 5);
  }
  */

  if (write) {
    cmd_header->attr |= (1 << 6);
  }

  // TODO: scatter list
  if (size > 0) {
    PhysRegionDesc* desc = &cmd_table->descriptors;
    desc->base_low = buffer;
    desc->base_high = 0;
    desc->reserved0 = 0;
    desc->byte_count = (size - 1) | (1 << 31);
  }

  CommandFIS_t* cmd_fis = &cmd_table->fis;

  memset(cmd_fis, 0, 64);

  cmd_fis->type = AHCI_FIS_TYPE_REG_H2D;
  cmd_fis->flags = 0x80;
  cmd_fis->command = cmd;
  cmd_fis->lba0 = lba & 0xFF;
  cmd_fis->lba1 = (lba >> 8) & 0xFF;
  cmd_fis->lba2 = (lba >> 16) & 0xFF;
  cmd_fis->lba3 = (lba >> 24) & 0xFF;
  cmd_fis->lba4 = (lba >> 32) & 0xFF;
  cmd_fis->lba5 = (lba >> 40) & 0xFF;
  cmd_fis->count = blk_count;
  cmd_fis->dev = 0x40;

  port->m_is_waiting = true;
  port->m_awaiting_dma_transfer_complete = true;
  port->m_transfer_failed = false;

  uint32_t ci = ahci_port_mmio_read32(port, AHCI_REG_PxCI);
  ci |= (1 << cmd_header_idx);
  ahci_port_mmio_write32(port, AHCI_REG_PxCI, ci);

  // FIXME: the write above here does not get accepted?
  // Qemu trace seems to show AHCI does do something with this, 
  // but there just is no response in the form of an interrupt...
  return ANIVA_SUCCESS;
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

ahci_port_t* make_ahci_port(struct ahci_device* device, uintptr_t port_offset, uint32_t index) {

  uintptr_t ib_page = kmem_prepare_new_physical_page().m_ptr;

  ahci_port_t* ret = kmalloc(sizeof(ahci_port_t));
  ret->m_port_index = index;
  ret->m_device = device;
  ret->m_port_offset = port_offset;
  ret->m_ib_page = ib_page;

  ret->m_hard_lock = create_spinlock();
  ret->m_awaiting_dma_transfer_complete = false;
  ret->m_transfer_failed = false;
  ret->m_is_waiting = false;

  // prepare buffers
  ret->m_fis_recieve_page = (paddr_t)kmem_kernel_alloc_extended(Release(kmem_request_physical_page()), SMALL_PAGE_SIZE, 0, KMEM_FLAG_DMA);
  ret->m_cmd_list_page = (paddr_t)kmem_kernel_alloc_extended(Release(kmem_request_physical_page()), SMALL_PAGE_SIZE, 0, KMEM_FLAG_DMA);

  ret->m_dma_buffer = (paddr_t)kmem_kernel_alloc_extended(Release(kmem_request_physical_page()), SMALL_PAGE_SIZE, 0, KMEM_FLAG_DMA);
  ret->m_cmd_table_buffer = (paddr_t)kmem_kernel_alloc_extended(Release(kmem_request_physical_page()), SMALL_PAGE_SIZE, 0, KMEM_FLAG_DMA);

  return ret;
}

void destroy_ahci_port(ahci_port_t* port) {

  kmem_kernel_dealloc(port->m_dma_buffer, SMALL_PAGE_SIZE);
  kmem_kernel_dealloc(port->m_cmd_list_page, SMALL_PAGE_SIZE);
  kmem_kernel_dealloc(port->m_fis_recieve_page, SMALL_PAGE_SIZE);
  kmem_kernel_dealloc(port->m_cmd_table_buffer, SMALL_PAGE_SIZE);

  destroy_spinlock(port->m_hard_lock);
  kfree(port);
}

ANIVA_STATUS initialize_port(ahci_port_t *port) {

  if (!port_has_phy(port)) {
    println_kterm("No Phy");
    kernel_panic("No Phy");
    return ANIVA_FAIL;
  }

  // Reset errors
  ahci_port_mmio_write32(port, AHCI_REG_PxSERR, ahci_port_mmio_read32(port, AHCI_REG_PxSERR));

  port_set_sleeping(port);

  memset((void*)port->m_fis_recieve_page, 0, SMALL_PAGE_SIZE);
  memset((void*)port->m_cmd_list_page, 0, SMALL_PAGE_SIZE);

  paddr_t cmd_list = kmem_to_phys(nullptr, port->m_cmd_list_page);
  paddr_t fis_recieve = kmem_to_phys(nullptr, port->m_fis_recieve_page);

  // TODO: check for CAP.S64A so we can set CLBU and FBU accortingly
  ahci_port_mmio_write32(port, AHCI_REG_PxCLB, cmd_list);
  ahci_port_mmio_write32(port, AHCI_REG_PxCLBU, 0);
  ahci_port_mmio_write32(port, AHCI_REG_PxFB, fis_recieve);
  ahci_port_mmio_write32(port, AHCI_REG_PxFBU, 0);

  /*
  CommandHeader_t* headers = (CommandHeader_t*)cmd_list;

  for (uint32_t i = 0; i < 32; i++) {
    headers[i].phys_region_desc_table_lenght = 8;
    headers[i].command_table_base_addr = (uintptr_t)kmem_kernel_alloc(Release(kmem_request_physical_page()), SMALL_PAGE_SIZE, KMEM_CUSTOMFLAG_IDENTITY);
    headers[i].command_table_base_addr_upper = 0;
  }
  */

  // Reset errors
  ahci_port_mmio_write32(port, AHCI_REG_PxSERR, ahci_port_mmio_read32(port, AHCI_REG_PxSERR));

  // Reset interrupts
  ahci_port_mmio_write32(port, AHCI_REG_PxIE, 0);
  ahci_port_mmio_write32(port, AHCI_REG_PxIS, ahci_port_mmio_read32(port, AHCI_REG_PxIS));

  uint32_t tfd = ahci_port_mmio_read32(port, AHCI_REG_PxTFD);

  if ((tfd & (AHCI_PxTFD_DRQ | AHCI_PxTFD_BSY)) == 0) {
    if (port_has_phy(port)) {
      // Device connected, let's ask it for it's info

      return ANIVA_SUCCESS;
    }
  }

  port_stop_fis_recieving(port);

  return ANIVA_FAIL;
}

ANIVA_STATUS ahci_port_gather_info(ahci_port_t* port) {

  port_set_active(port);

  ahci_port_mmio_write32(port, AHCI_REG_PxIE, AHCI_PORT_INTERRUPT_FULL_ENABLE);

  ata_identify_block_t* dev_identify_buffer = (ata_identify_block_t*)Release(kmem_kernel_alloc_range(SMALL_PAGE_SIZE, 0, KMEM_FLAG_DMA));
  paddr_t dev_ident_phys = kmem_to_phys(nullptr, (vaddr_t)dev_identify_buffer);

  println("Waiting for cmd...");
  if (ahci_port_send_command(port, AHCI_COMMAND_IDENTIFY_DEVICE, dev_ident_phys, 512, false, 0, 0) == ANIVA_FAIL || ahci_port_await_dma_completion_sync(port) == ANIVA_FAIL) {
    goto fail_and_dealloc;
  }

  if (dev_identify_buffer->general_config.dev_type) {
    goto fail_and_dealloc;
  }

  // Default sector size
  port->m_logical_sector_size = 512;
  port->m_physical_sector_size = 512;

  uint16_t psstlss = dev_identify_buffer->physical_sector_size_to_logical_sector_size;

  if ((psstlss >> 14) == 1) {
    if (psstlss & (1 << 12)) {
      port->m_logical_sector_size = dev_identify_buffer->logical_sector_size;
    }
    if (psstlss & (1 << 13)) {
      port->m_physical_sector_size = port->m_logical_sector_size << (psstlss & 0xF);
    }
  }
  if (dev_identify_buffer->commands_and_feature_sets_supported[1] & (1 << 10)) {
    port->m_max_sector = dev_identify_buffer->user_addressable_logical_sectors_count;
  } else {
    port->m_max_sector = dev_identify_buffer->max_28_bit_addressable_logical_sector;
  }

  // TODO: Save 
  const char* model_num = (const char*)dev_identify_buffer->model_number;

  println_kterm("");
  println_kterm(model_num);
  println_kterm("");

  println_kterm("Logical sector size: ");
  println_kterm(to_string(port->m_logical_sector_size));
  println_kterm("Physical sector size: ");
  println_kterm(to_string(port->m_physical_sector_size));

  println_kterm("Recieved the thing");

  return ANIVA_SUCCESS;

fail_and_dealloc:
  kernel_panic("DEV IDENTIFY WENT WRONG");
  kmem_kernel_dealloc((vaddr_t)dev_identify_buffer, SMALL_PAGE_SIZE);
  return ANIVA_FAIL;
}

ANIVA_STATUS ahci_port_handle_int(ahci_port_t* port) {

  // TODO:
  println("Port could handle AHCI interrupt");
  port->m_awaiting_dma_transfer_complete = false;

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

  while (ahci_port_mmio_read32(port, AHCI_REG_PxCMD) & AHCI_PxCMD_CR)
    delay(500);

  uint32_t cmd_and_status = ahci_port_mmio_read32(port, AHCI_REG_PxCMD);
  cmd_and_status |= AHCI_PxCMD_ST;
  cmd_and_status |= AHCI_PxCMD_FRE;
  ahci_port_mmio_write32(port, AHCI_REG_PxCMD, cmd_and_status);
}
static ALWAYS_INLINE void port_set_sleeping(ahci_port_t* port) {

  // Zero CMD.ST
  uint32_t cmd_and_status = ahci_port_mmio_read32(port, AHCI_REG_PxCMD);
  cmd_and_status &= ~AHCI_PxCMD_ST;
  ahci_port_mmio_write32(port, AHCI_REG_PxCMD, cmd_and_status);

  // Wait for CMD.CR to clear
  while (cmd_and_status & AHCI_PxCMD_CR) {
    cmd_and_status = ahci_port_mmio_read32(port, AHCI_REG_PxCMD);
  }

  // Finally zero CMD.FRE
  cmd_and_status &= ~AHCI_PxCMD_FRE;
  ahci_port_mmio_write32(port, AHCI_REG_PxCMD, cmd_and_status);

  // TODO: Spec states software should wait at least 500 miliseconds. We 
  // can abort after this time has passed + some buffer time perhaps
  while (true) {
    cmd_and_status = ahci_port_mmio_read32(port, AHCI_REG_PxCMD);

    if ((cmd_and_status & AHCI_PxCMD_FR) || (cmd_and_status & AHCI_PxCMD_CR))
      continue;

    break;
  }
}
static ALWAYS_INLINE bool port_has_phy(ahci_port_t* port) {
  uint32_t sata_stat = ahci_port_mmio_read32(port, AHCI_REG_PxSSTS);
  return (sata_stat & 0x0f) == 3;
}
