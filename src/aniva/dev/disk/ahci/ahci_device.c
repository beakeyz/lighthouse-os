#include "ahci_device.h"
#include "dev/core.h"
#include "dev/debug/serial.h"
#include "dev/disk/ahci/ahci_port.h"
#include "dev/disk/ahci/definitions.h"
#include "dev/disk/generic.h"
#include "dev/kterm/kterm.h"
#include "dev/pci/definitions.h"
#include "dev/pci/pci.h"
#include "interrupts/control/interrupt_control.h"
#include "interrupts/interrupts.h"
#include "libk/atomic.h"
#include "libk/flow/error.h"
#include "libk/data/hive.h"
#include "libk/io.h"
#include "libk/data/linkedlist.h"
#include "libk/stddef.h"
#include "libk/string.h"
#include "dev/pci/io.h"
#include "proc/ipc/packet_response.h"
#include "sched/scheduler.h"
#include <mem/heap.h>
#include <mem/kmem_manager.h>
#include <crypto/k_crc32.h>

static ahci_device_t* __ahci_devices;

static pci_dev_id_t ahci_id_table[] = {
  PCI_DEVID_CLASSES(MASS_STORAGE, PCI_SUBCLASS_SATA, PCI_PROGIF_SATA),
  PCI_DEVID_END
};

static ALWAYS_INLINE void* get_hba_region(ahci_device_t* device);
static ALWAYS_INLINE ANIVA_STATUS reset_hba(ahci_device_t* device);
static ALWAYS_INLINE ANIVA_STATUS initialize_hba(ahci_device_t* device);
static ALWAYS_INLINE ANIVA_STATUS set_hba_interrupts(ahci_device_t* device, bool value);

static ALWAYS_INLINE registers_t* ahci_irq_handler(registers_t *regs);

static void __register_ahci_device(ahci_device_t* device)
{
  device->m_next = __ahci_devices;
  __ahci_devices = device;
}

static void __unregister_ahci_device(ahci_device_t* device)
{
  ahci_device_t** target;

  for (target = &__ahci_devices; *target; target = &(*target)->m_next) {

    if (*target == device) {
      *target = device->m_next;
      device->m_next = nullptr;

      return;
    }
  }
}

uint32_t ahci_mmio_read32(uintptr_t base, uintptr_t offset) {
  return *(volatile uint32_t*)(base + offset);
}

void ahci_mmio_write32(uintptr_t base, uintptr_t offset, uint32_t data) {
  volatile uint32_t* current_data = (volatile uint32_t*)(base + offset);
  *current_data = data;
}

static ALWAYS_INLINE void* get_hba_region(ahci_device_t* device) {

  const uint32_t bus_num = device->m_identifier->address.bus_num;
  const uint32_t device_num = device->m_identifier->address.device_num;
  const uint32_t func_num = device->m_identifier->address.func_num;
  uint32_t bar5;
  g_pci_type1_impl.read32(bus_num, device_num, func_num, BAR5, &bar5);
  
  uintptr_t bar_addr = get_bar_address(bar5);

  uintptr_t hba_region = (uintptr_t)Must(__kmem_kernel_alloc(bar_addr, ALIGN_UP(sizeof(HBA), SMALL_PAGE_SIZE * 2), 0, KMEM_FLAG_WC | KMEM_FLAG_KERNEL | KMEM_FLAG_WRITABLE));

  return (void*)hba_region;
}

// TODO: redo this
static ALWAYS_INLINE ANIVA_STATUS reset_hba(ahci_device_t* device) {

  // get this mofo up and running and disable interrupts
  ahci_mmio_write32((uintptr_t)device->m_hba_region, AHCI_REG_AHCI_GHC, 
      (ahci_mmio_read32((uintptr_t)device->m_hba_region, AHCI_REG_AHCI_GHC) | AHCI_GHC_AE) & ~(AHCI_GHC_IE));;

  uint32_t pi = device->m_hba_region->control_regs.ports_impl;

  uintptr_t internal_index = 0;
  for (uint32_t i = 0; i < MAX_HBA_PORT_AMOUNT; i++) {
    if (pi & (1 << i)) {

      uintptr_t port_offset = 0x100 + i * 0x80;


      volatile HBA_port_registers_t* port_regs = ((volatile HBA_port_registers_t*)&device->m_hba_region->ports[i]);

      uint32_t sig = ahci_mmio_read32((uintptr_t)device->m_hba_region + port_offset, AHCI_REG_PxSIG);

      bool valid = false;

      // TODO: take these sigs out of this 
      // function scope and into a macro
      switch(sig) {
        case 0xeb140101:
          println("ATAPI (CD, DVD)");
          break;
        case 0x00000101:
          println("AHCI: hard drive");
          valid = true;
          break;
        case 0xffff0101:
          println("AHCI: no dev");
          break;
        default:
          print("ACHI: unsupported signature ");
          println(to_string(port_regs->signature));
          break;
      }

      if (!valid) {
        continue;
      }

      ahci_port_t* port = create_ahci_port(device, (uintptr_t)device->m_hba_region + port_offset, internal_index);
      if (initialize_port(port) == ANIVA_FAIL) {
        println("Failed! killing port...");
        destroy_ahci_port(port);
        continue;
      }

      /* Register the global disk device */
      register_gdisk_dev(&port->m_generic);

      hive_add_entry(device->m_ports, port, port->m_generic.m_path);

      internal_index++;
    }
  }

  return ANIVA_SUCCESS;
}

static bool port_itterator(hive_t* current, void* data) {

  ANIVA_STATUS status;
  ahci_port_t* port = data;

  status = ahci_port_gather_info(port);

  if (status == ANIVA_FAIL) {
    println("Failed to gather info!");
    return false;
  }
  return true;
}

static ALWAYS_INLINE ANIVA_STATUS initialize_hba(ahci_device_t* device) {

  // enable hba interrupts
  uint16_t bus_num = device->m_identifier->address.bus_num;
  uint16_t dev_num = device->m_identifier->address.device_num;
  uint16_t func_num = device->m_identifier->address.func_num;
  //pci_set_interrupt_line(&device->m_identifier->address, true);
  //pci_set_bus_mastering(&device->m_identifier->address, true);
  //pci_set_memory(&device->m_identifier->address, true);

  uint8_t interrupt_line;
  device->m_identifier->raw_ops.read8(bus_num, dev_num, func_num, INTERRUPT_LINE, &interrupt_line);

  device->m_hba_region = get_hba_region(__ahci_devices);

  // We might need to fetch AHCI from the BIOS
  if (ahci_mmio_read32((uintptr_t)device->m_hba_region, AHCI_REG_CAP2) & AHCI_CAP2_BOH) {
    uint32_t bohc = ahci_mmio_read32((uintptr_t)device->m_hba_region, AHCI_REG_BOHC) | AHCI_BOHC_OOS;
    ahci_mmio_write32((uintptr_t)device->m_hba_region, AHCI_REG_BOHC, bohc);

    while (ahci_mmio_read32((uintptr_t)device->m_hba_region, AHCI_REG_BOHC) & (AHCI_BOHC_BOS | AHCI_BOHC_BB)) {
      delay(100);
    }
  }

  ANIVA_STATUS status = reset_hba(device);

  if (status != ANIVA_SUCCESS)
    return status;

  // HBA has been reset, enable its interrupts and claim this line
  ErrorOrPtr result = install_quick_int_handler(interrupt_line, QIH_FLAG_REGISTERED | QIH_FLAG_BLOCK_EVENTS, I8259, ahci_irq_handler);

  if (result.m_status == ANIVA_SUCCESS)
    quick_int_handler_enable_vector(interrupt_line);

  uint32_t ghc = ahci_mmio_read32((uintptr_t)device->m_hba_region, AHCI_REG_AHCI_GHC) | AHCI_GHC_IE;
  ahci_mmio_write32((uintptr_t)device->m_hba_region, AHCI_REG_AHCI_GHC, ghc);

  println("Gathering info about ports...");

  if (hive_walk(device->m_ports, true, port_itterator).m_status == ANIVA_FAIL)
    return ANIVA_FAIL;

  println("Done!");
  return status;
}

static ALWAYS_INLINE ANIVA_STATUS set_hba_interrupts(ahci_device_t* device, bool value) {
  if (value) {
    device->m_hba_region->control_regs.global_host_ctrl |= AHCI_GHC_IE; // 0x02
  } else {
    device->m_hba_region->control_regs.global_host_ctrl &= ~AHCI_GHC_IE;
  }
  return ANIVA_SUCCESS;
}

static ALWAYS_INLINE registers_t* ahci_irq_handler(registers_t *regs) {

  // TODO: handle
  kernel_panic("Recieved AHCI interrupt!");

  uint32_t ahci_interrupt_status = __ahci_devices->m_hba_region->control_regs.int_status;

  if (ahci_interrupt_status == 0) {
    return regs;
  }

  for (uint32_t i = 0; i < 32; i++) {
    if (ahci_interrupt_status & (1 << i)) {
      char port_path[strlen("prt") + strlen(to_string(i)) + 1];
      concat("prt", (char*)to_string(i), (char*)port_path);
      ahci_port_t* port = hive_get(__ahci_devices->m_ports, port_path);
      
      ahci_port_handle_int(port); 
    }
  }

  return regs;
}

// TODO:
ahci_device_t* create_ahci_device(pci_device_t* identifier) {
  ahci_device_t* ahci_device = kmalloc(sizeof(ahci_device_t));

  memset(ahci_device, 0, sizeof(ahci_device_t));

  __register_ahci_device(ahci_device);

  ahci_device->m_identifier = identifier;
  ahci_device->m_ports = create_hive("ports");

  if (initialize_hba(ahci_device) == ANIVA_FAIL) {
    destroy_ahci_device(ahci_device);
    return nullptr;
  }

  return ahci_device;
}

static bool __destroy_device_ports(hive_t* root, void* port)
{
  destroy_ahci_port(port);
  return true;
}

void destroy_ahci_device(ahci_device_t* device) {

  __unregister_ahci_device(device);

  hive_walk(device->m_ports, true, __destroy_device_ports);
  destroy_hive(device->m_ports);

  kfree(device);
}

ahci_dch_t* create_ahci_command_header(void* buffer, size_t size, disk_offset_t offset) {
  ahci_dch_t* header = kmalloc(sizeof(ahci_dch_t));

  // TODO
  header->m_flags = 0;
  header->m_crc = 0;

  header->m_req_buffer = buffer;
  header->m_req_size = size;
  header->m_req_offset = offset;

  header->m_crc = kcrc32(header, sizeof(ahci_dch_t));

  return header;
}

void destroy_ahci_command_header(ahci_dch_t* header) {
  // kfree(header->m_req_buffer);
  kfree(header);
}

ErrorOrPtr ahci_cmd_header_check_crc(ahci_dch_t* header) {

  uintptr_t crc_check;
  ahci_dch_t copy = *header;
  copy.m_crc = 0;

  crc_check = kcrc32(&copy, sizeof(ahci_dch_t));

  if (crc_check == header->m_crc) {
    return Success(0);
  }

  return Error();
}

static int ahci_probe(pci_device_t* identifier, pci_driver_t* driver) {
  ahci_device_t* ahci_device = create_ahci_device(identifier);

  if (!ahci_device)
    return -1;

  return 0;
}

pci_driver_t ahci_pci_driver = {
  .id_table = ahci_id_table,
  .f_probe = ahci_probe,
};

int ahci_driver_init() {

  // FIXME: should this driver really take a hold of the scheduler 
  // here?
  __ahci_devices = nullptr;

  register_pci_driver(&ahci_pci_driver);

  return 0;
}

int ahci_driver_exit() {
  // shutdown ahci device

  println("Shut down ahci driver");

  if (__ahci_devices) {
    destroy_ahci_device(__ahci_devices);
  }

  unregister_pci_driver(&ahci_pci_driver);

  return 0;
}

uintptr_t ahci_driver_on_packet(packet_payload_t payload, packet_response_t** response) {

  if (__ahci_devices == nullptr) {
    return (uintptr_t)-1;
  }

  switch (payload.m_code) {
    case AHCI_MSG_GET_PORT: {
      const char* path = (const char*)payload.m_data;

      ahci_port_t* res = hive_get(__ahci_devices->m_ports, path);

      ahci_port_t copy = *res;

      if (res) {
        *response = create_packet_response(&copy, sizeof(ahci_port_t));
      }
      break;
    }
    case AHCI_MSG_READ: {
      ahci_dch_t* header = (ahci_dch_t*)payload.m_data;

      if (ahci_cmd_header_check_crc(header).m_status == ANIVA_FAIL) {
        kernel_panic("FUCK ahci command header does not have a matching crc32!");
        return (uintptr_t)-1;
      }

      ahci_port_t* port = hive_get(__ahci_devices->m_ports, header->m_port_path);

      int result = port->m_generic.m_ops.f_read_sync(&port->m_generic, header->m_req_buffer, header->m_req_size, header->m_req_offset);

      if (result < 0) {
        return (uintptr_t)-1;
      }

      *response = create_packet_response(header, sizeof(ahci_dch_t));
      return 0;
    }
  }

  return 0;
}


aniva_driver_t base_ahci_driver = {
  .m_name = "ahci",
  .m_type = DT_DISK,
  .m_version = DRIVER_VERSION(0, 0, 1),
  .f_init = ahci_driver_init,
  .f_exit = ahci_driver_exit,
  .f_drv_msg = ahci_driver_on_packet,
  /*
   * FIXME: when we insert a dependency here that is deferred (a socket), we completely die, since this 
   * non-socket driver will fail to load as it realizes that it is unable to load its dependency...
   */
  .m_dep_count = 0
};
EXPORT_DRIVER_PTR(base_ahci_driver);
