#include "ahci_device.h"
#include "dev/core.h"
#include "dev/debug/serial.h"
#include "dev/disk/ahci/ahci_port.h"
#include "dev/disk/ahci/definitions.h"
#include "dev/disk/generic.h"
#include "dev/kterm/kterm.h"
#include "dev/pci/definitions.h"
#include "dev/pci/pci.h"
#include "interupts/control/interrupt_control.h"
#include "interupts/interupts.h"
#include "libk/async_ptr.h"
#include "libk/atomic.h"
#include "libk/error.h"
#include "libk/hive.h"
#include "libk/io.h"
#include "libk/linkedlist.h"
#include "libk/stddef.h"
#include "libk/string.h"
#include "dev/pci/io.h"
#include "proc/ipc/packet_response.h"
#include "sched/scheduler.h"
#include <mem/heap.h>
#include <mem/kmem_manager.h>
#include <crypto/k_crc32.h>

#include <dev/framebuffer/framebuffer.h>

int ahci_driver_init();
int ahci_driver_exit();
uintptr_t ahci_driver_on_packet(packet_payload_t payload, packet_response_t** response);

static ahci_device_t* s_ahci_device;
static size_t s_implemented_ports;

static char* s_last_debug_msg;

const aniva_driver_t g_base_ahci_driver = {
  .m_name = "ahci",
  .m_type = DT_DISK,
  .m_ident = DRIVER_IDENT(0, 0),
  .m_version = DRIVER_VERSION(0, 0, 1),
  .f_init = ahci_driver_init,
  .f_exit = ahci_driver_exit,
  .f_drv_msg = ahci_driver_on_packet,
  .m_dependencies = {"graphics/kterm"},
  .m_dep_count = 1
};
EXPORT_DRIVER(g_base_ahci_driver);

static ALWAYS_INLINE void* get_hba_region(ahci_device_t* device);
static ALWAYS_INLINE ANIVA_STATUS reset_hba(ahci_device_t* device);
static ALWAYS_INLINE ANIVA_STATUS initialize_hba(ahci_device_t* device);
static ALWAYS_INLINE ANIVA_STATUS set_hba_interrupts(ahci_device_t* device, bool value);

static ALWAYS_INLINE registers_t* ahci_irq_handler(registers_t *regs);

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

  print("BAR5 address: ");
  println(to_string(bar5));

  uintptr_t hba_region = (uintptr_t)Must(__kmem_kernel_alloc(bar5, ALIGN_UP(sizeof(HBA), SMALL_PAGE_SIZE * 2), KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE, KMEM_FLAG_WC | KMEM_FLAG_KERNEL | KMEM_FLAG_WRITABLE));

  println("Got the address");
  println(to_string(hba_region));

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

      println_kterm("Found HBA port");
      ahci_port_t* port = create_ahci_port(device, (uintptr_t)device->m_hba_region + port_offset, internal_index);
      if (initialize_port(port) == ANIVA_FAIL) {
        println("Failed! killing port...");
        println_kterm("Failed to initialize port");
        destroy_ahci_port(port);
        continue;
      }
      register_gdisk_dev(&port->m_generic);
      hive_add_entry(device->m_ports, port, port->m_generic.m_path);
      s_implemented_ports++;
      internal_index++;
    }
  }

  s_last_debug_msg = "Exited port enumeration";
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
  device->m_identifier->ops.read8(bus_num, dev_num, func_num, INTERRUPT_LINE, &interrupt_line);

  device->m_hba_region = get_hba_region(s_ahci_device);

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

  // HBA has been reset, enable its interrupts
  InterruptHandler_t* _handler = create_interrupt_handler(interrupt_line, I8259, ahci_irq_handler);
  bool result = interrupts_add_handler(_handler);

  if (result)
    _handler->m_controller->fControllerEnableVector(interrupt_line);

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

  uint32_t ahci_interrupt_status = s_ahci_device->m_hba_region->control_regs.int_status;

  if (ahci_interrupt_status == 0) {
    return regs;
  }

  for (uint32_t i = 0; i < 32; i++) {
    if (ahci_interrupt_status & (1 << i)) {
      char port_path[strlen("prt") + strlen(to_string(i)) + 1];
      concat("prt", (char*)to_string(i), (char*)port_path);
      ahci_port_t* port = hive_get(s_ahci_device->m_ports, port_path);
      
      ahci_port_handle_int(port); 
    }
  }

  return regs;
}

// TODO:
ahci_device_t* init_ahci_device(pci_device_identifier_t* identifier) {
  s_ahci_device = kmalloc(sizeof(ahci_device_t));
  s_ahci_device->m_identifier = identifier;
  s_ahci_device->m_ports = create_hive("ports");

  initialize_hba(s_ahci_device);

  return s_ahci_device;
}

void destroy_ahci_device(ahci_device_t* device) {
  // TODO:
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

static void find_ahci_device(pci_device_identifier_t* identifier) {

  if (identifier->class == MASS_STORAGE) {
    // 0x01 == AHCI progIF
    if (identifier->subclass == PCI_SUBCLASS_SATA && identifier->prog_if == PCI_PROGIF_SATA) {
      if (s_ahci_device != nullptr) {
        println("Found second AHCI device?");
        return;
      }
      // ahci controller
      s_ahci_device = init_ahci_device(identifier);
    }
  }
}

int ahci_driver_init() {

  // FIXME: should this driver really take a hold of the scheduler 
  // here?
  s_last_debug_msg = "None";
  s_ahci_device = 0;
  s_implemented_ports = 0;
  
  enumerate_registerd_devices(find_ahci_device);

  /*
  char* number_of_ports = (char*)to_string(s_ahci_device->m_ports->m_length);
  char* message = "\nimplemented ports: ";
  const char* packet_message = concat(message, number_of_ports);
  destroy_packet_response(driver_send_packet_sync("graphics.kterm", KTERM_DRV_DRAW_STRING, (void**)&packet_message, strlen(packet_message)));
  destroy_packet_response(driver_send_packet_sync("graphics.kterm", KTERM_DRV_DRAW_STRING, (void**)&s_last_debug_msg, strlen(packet_message)));
  kfree((void*)packet_message);
  */
  return 0;
}

int ahci_driver_exit() {
  // shutdown ahci device

  println("Shut down ahci driver");

  if (s_ahci_device) {
    destroy_ahci_device(s_ahci_device);
  }
  s_implemented_ports = 0;

  return 0;
}

uintptr_t ahci_driver_on_packet(packet_payload_t payload, packet_response_t** response) {

  if (s_ahci_device == nullptr) {
    return (uintptr_t)-1;
  }

  switch (payload.m_code) {
    case AHCI_MSG_GET_PORT: {
      const char* path = (const char*)payload.m_data;

      ahci_port_t* res = hive_get(s_ahci_device->m_ports, path);

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

      ahci_port_t* port = hive_get(s_ahci_device->m_ports, header->m_port_path);

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
