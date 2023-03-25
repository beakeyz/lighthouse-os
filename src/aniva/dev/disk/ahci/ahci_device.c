#include "ahci_device.h"
#include "dev/core.h"
#include "dev/debug/serial.h"
#include "dev/disk/ahci/ahci_port.h"
#include "dev/disk/ahci/definitions.h"
#include "dev/kterm/kterm.h"
#include "dev/pci/definitions.h"
#include "dev/pci/pci.h"
#include "interupts/control/interrupt_control.h"
#include "interupts/interupts.h"
#include "libk/async_ptr.h"
#include "libk/atomic.h"
#include "libk/error.h"
#include "libk/io.h"
#include "libk/linkedlist.h"
#include "libk/stddef.h"
#include "libk/string.h"
#include "dev/pci/io.h"
#include "proc/ipc/packet_response.h"
#include "sched/scheduler.h"
#include <mem/heap.h>
#include <mem/kmem_manager.h>

#include <dev/framebuffer/framebuffer.h>

void ahci_driver_init();
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
  .m_port = 5,
  .m_dependencies = {},
  .m_dep_count = 0
};

static ALWAYS_INLINE void* get_hba_region(ahci_device_t* device);
static ALWAYS_INLINE ANIVA_STATUS reset_hba(ahci_device_t* device);
static ALWAYS_INLINE ANIVA_STATUS initialize_hba(ahci_device_t* device);
static ALWAYS_INLINE ANIVA_STATUS set_hba_interrupts(ahci_device_t* device, bool value);

static ALWAYS_INLINE registers_t* ahci_irq_handler(registers_t *regs);

uint32_t ahci_mmio_read32(uintptr_t base, uintptr_t offset) {
  volatile uint32_t* data = (volatile uint32_t*)(base + offset);

  return *data;
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

  uintptr_t hba_region = (uintptr_t)kmem_kernel_alloc_extended(bar5, ALIGN_UP(sizeof(HBA), SMALL_PAGE_SIZE * 2), KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE | KMEM_CUSTOMFLAG_IDENTITY, KMEM_FLAG_WC | KMEM_FLAG_KERNEL | KMEM_FLAG_WRITABLE) & 0xFFFFFFF0;

  return (void*)hba_region;
}

// TODO: redo this
static ALWAYS_INLINE ANIVA_STATUS reset_hba(ahci_device_t* device) {

  // NOTE: 0x01 is the ghc enable flag
  for (size_t retry = 0;; retry++) {

    if (retry > 1000) {
      s_last_debug_msg = "GHC timed out!";
      return ANIVA_FAIL;
    }

    if (!(device->m_hba_region->control_regs.global_host_ctrl & (1 << 0))) {
      break;
    }
    device->m_hba_region->control_regs.global_host_ctrl |= (1 << 0);
    delay(1000);
  }

  // get this mofo up and running
  device->m_hba_region->control_regs.global_host_ctrl |= (1 << 31UL) | AHCI_GHC_IE;

  uint32_t pi = device->m_hba_region->control_regs.ports_impl;

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
      ahci_port_t* port = make_ahci_port(device, (uintptr_t)device->m_hba_region + port_offset, i);
      if (initialize_port(port) == ANIVA_FAIL) {
        println("Failed! killing port...");
        println_kterm("Failed to initialize port");
        destroy_ahci_port(port);
        continue;
      }
      list_append(device->m_ports, port);
      s_implemented_ports++;
    }
  }

  s_last_debug_msg = "Exited port enumeration";
  return ANIVA_SUCCESS;
}

static ALWAYS_INLINE ANIVA_STATUS initialize_hba(ahci_device_t* device) {

  // enable hba interrupts
  uint16_t bus_num = device->m_identifier->address.bus_num;
  uint16_t dev_num = device->m_identifier->address.device_num;
  uint16_t func_num = device->m_identifier->address.func_num;
  uint8_t interrupt_line = device->m_identifier->interrupt_line;
  uint16_t command;
  device->m_identifier->ops.read16(bus_num, dev_num, func_num, COMMAND, &command);
  command |= PCI_COMMAND_BUS_MASTER;
  command |= PCI_COMMAND_MEM_SPACE;
  command ^= PCI_COMMAND_INT_DISABLE;
  device->m_identifier->ops.write16(bus_num, dev_num, func_num, COMMAND, command);

  device->m_hba_region = get_hba_region(s_ahci_device);

  // Disable interrupts for the HBA
  uint32_t ghc = ahci_mmio_read32((uintptr_t)device->m_hba_region, AHCI_REG_AHCI_GHC) & ~(AHCI_GHC_IE);
  ahci_mmio_write32((uintptr_t)device->m_hba_region, AHCI_REG_AHCI_GHC, ghc);
  //set_hba_interrupts(device, false);

  ANIVA_STATUS status = reset_hba(device);

  if (status != ANIVA_SUCCESS)
    return status;

  // HBA has been reset, enable its interrupts
  InterruptHandler_t* _handler = create_interrupt_handler(interrupt_line, I8259, ahci_irq_handler);
  bool result = interrupts_add_handler(_handler);

  if (result)
    _handler->m_controller->fControllerEnableVector(interrupt_line);

  ghc = ahci_mmio_read32((uintptr_t)device->m_hba_region, AHCI_REG_AHCI_GHC) | AHCI_GHC_IE;
  ahci_mmio_write32((uintptr_t)device->m_hba_region, AHCI_REG_AHCI_GHC, ghc);

  println("Gathering info about ports...");
  FOREACH(i, device->m_ports) {
    ahci_port_t* port = i->data;

    status = ahci_port_gather_info(port);

    if (status == ANIVA_FAIL) {
      println("Failed to gather info!");
      return status;
    }
  }

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
      ahci_port_t* port = list_get(s_ahci_device->m_ports, i);
      
      ahci_port_handle_int(port); 
    }
  }

  return regs;
}

// TODO:
ahci_device_t* init_ahci_device(pci_device_identifier_t* identifier) {
  s_ahci_device = kmalloc(sizeof(ahci_device_t));
  s_ahci_device->m_identifier = identifier;
  s_ahci_device->m_ports = init_list();

  initialize_hba(s_ahci_device);

  return s_ahci_device;
}

void destroy_ahci_device(ahci_device_t* device) {
  // TODO:
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

void ahci_driver_init() {

  // FIXME: should this driver really take a hold of the scheduler 
  // here?
  s_last_debug_msg = "None";
  s_ahci_device = 0;
  s_implemented_ports = 0;
  
  enumerate_registerd_devices(find_ahci_device);

  char* number_of_ports = (char*)to_string(s_ahci_device->m_ports->m_length);
  char* message = "\nimplemented ports: ";
  const char* packet_message = concat(message, number_of_ports);
  destroy_packet_response(driver_send_packet_sync("graphics.kterm", KTERM_DRV_DRAW_STRING, (void**)&packet_message, strlen(packet_message)));
  destroy_packet_response(driver_send_packet_sync("graphics.kterm", KTERM_DRV_DRAW_STRING, (void**)&s_last_debug_msg, strlen(packet_message)));
  kfree((void*)packet_message);
}

int ahci_driver_exit() {
  // shutdown ahci device

  if (s_ahci_device) {
    destroy_ahci_device(s_ahci_device);
  }
  s_implemented_ports = 0;

  return 0;
}

uintptr_t ahci_driver_on_packet(packet_payload_t payload, packet_response_t** response) {
  return 0;
}
