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
static ALWAYS_INLINE volatile HBA* get_hba(ahci_device_t* device);
static ALWAYS_INLINE ANIVA_STATUS reset_hba(ahci_device_t* device);
static ALWAYS_INLINE ANIVA_STATUS initialize_hba(ahci_device_t* device);
static ALWAYS_INLINE ANIVA_STATUS set_hba_interrupts(ahci_device_t* device, bool value);

static ALWAYS_INLINE registers_t* ahci_irq_handler(registers_t *regs);

static ALWAYS_INLINE void* get_hba_region(ahci_device_t* device) {

  const uint32_t bus_num = device->m_identifier->address.bus_num;
  const uint32_t device_num = device->m_identifier->address.device_num;
  const uint32_t func_num = device->m_identifier->address.func_num;
  uint32_t bar5;
  g_pci_type1_impl.read32(bus_num, device_num, func_num, BAR5, &bar5);

  print("BAR5 address: ");
  println(to_string(bar5));

  uintptr_t hba_region = (uintptr_t)kmem_kernel_alloc(bar5, ALIGN_UP(sizeof(HBA), SMALL_PAGE_SIZE), KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE | KMEM_CUSTOMFLAG_IDENTITY) & 0xFFFFFFF0;

  return (void*)hba_region;
}

static ALWAYS_INLINE volatile HBA* get_hba(ahci_device_t* device) {
  return (volatile HBA*)device->m_hba_region;
}

// TODO: redo this
static ALWAYS_INLINE ANIVA_STATUS reset_hba(ahci_device_t* device) {

  // NOTE: 0x01 is the ghc enable flag
  for (size_t retry = 0;; retry++) {

    if (retry > 1000) {
      return ANIVA_FAIL;
    }

    if (!(get_hba(device)->control_regs.global_host_ctrl & 0x01)) {
      break;
    }
    get_hba(device)->control_regs.global_host_ctrl |= 0x01;
    delay(1000);
  }

  // get this mofo up and running
  get_hba(device)->control_regs.global_host_ctrl |= (1 << 31);
  get_hba(device)->control_regs.int_status = 0xffffffff;

  uint32_t pi = get_hba(device)->control_regs.ports_impl;

  for (uint32_t i = 0; i < MAX_HBA_PORT_AMOUNT; i++) {
    if (pi & (1 << i)) {
      print("HBA port ");
      print(to_string(i));
      println(" is implemented");

      volatile HBA_port_registers_t* port_regs = ((volatile HBA_port_registers_t*)&get_hba(device)->ports[i]);

      switch(port_regs->signature) {
        case 0xeb140101:
          println("ATAPI (CD, DVD)");
          break;
        case 0x00000101:
          println("AHCI: hard drive");
          break;
        case 0xffff0101:
          println("AHCI: no dev");
          break;
        default:
          print("ACHI: unsupported signature ");
          println(to_string(port_regs->signature));
          break;
      }

      ahci_port_t* port = make_ahci_port(device, port_regs, i);
      if (initialize_port(port) == ANIVA_FAIL) {
        println("Failed! killing port...");
        destroy_ahci_port(port);
        continue;
      }
      list_append(device->m_ports, port);
      s_implemented_ports++;
    }
  }

  return ANIVA_FAIL;
}

static ALWAYS_INLINE ANIVA_STATUS initialize_hba(ahci_device_t* device) {

  // enable hba interrupts
  pci_set_interrupt_line(&device->m_identifier->address, true);
  pci_set_bus_mastering(&device->m_identifier->address, true);
  pci_set_memory(&device->m_identifier->address, true);

  get_hba(device)->control_regs.global_host_ctrl = 0x80000000;
  
  set_hba_interrupts(device, true);

  InterruptHandler_t* _handler = create_interrupt_handler(device->m_identifier->interrupt_line, I8259, ahci_irq_handler);
  interrupts_add_handler(_handler);

  return reset_hba(device);
}

static ALWAYS_INLINE ANIVA_STATUS set_hba_interrupts(ahci_device_t* device, bool value) {
  uint32_t ghc = get_hba(device)->control_regs.global_host_ctrl;
  if (value) {
    get_hba(device)->control_regs.global_host_ctrl = ghc | (1 << 1); // 0x02
  } else {
    get_hba(device)->control_regs.global_host_ctrl = ghc & ~(1 << 1);
  }
  return ANIVA_SUCCESS;
}

static ALWAYS_INLINE registers_t* ahci_irq_handler(registers_t *regs) {

  println("Recieved AHCI interrupt!");

  uint32_t ahci_interrupt_status = get_hba(s_ahci_device)->control_regs.int_status;

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
  s_ahci_device->m_hba_region = get_hba_region(s_ahci_device);
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
  pause_scheduler();

  s_ahci_device = 0;
  s_implemented_ports = 0;
  
  enumerate_registerd_devices(find_ahci_device);

  resume_scheduler();

  char* number_of_ports = (char*)to_string(s_ahci_device->m_ports->m_length);
  char* message = "\nimplemented ports: ";
  const char* packet_message = concat(message, number_of_ports);
  destroy_packet_response(driver_send_packet_sync("graphics.kterm", KTERM_DRV_DRAW_STRING, (void**)&packet_message, strlen(packet_message)));
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
