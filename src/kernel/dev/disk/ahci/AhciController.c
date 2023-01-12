#include "AhciController.h"
#include "dev/debug/serial.h"
#include "dev/disk/ahci/AhciPort.h"
#include "dev/disk/ahci/definitions.h"
#include "dev/pci/pci.h"
#include "interupts/control/interrupt_control.h"
#include "interupts/interupts.h"
#include "libk/atomic.h"
#include "libk/error.h"
#include "libk/io.h"
#include "libk/linkedlist.h"
#include "libk/stddef.h"
#include "libk/string.h"
#include <mem/kmalloc.h>
#include <mem/kmem_manager.h>

static AhciDevice_t* s_ahci_device = nullptr;

static ALWAYS_INLINE void* get_hba_region(AhciDevice_t* device);
static ALWAYS_INLINE volatile HBA* get_hba(AhciDevice_t* device);
static ALWAYS_INLINE LIGHT_STATUS reset_hba(AhciDevice_t* device);
static ALWAYS_INLINE LIGHT_STATUS initialize_hba(AhciDevice_t* device);
static ALWAYS_INLINE LIGHT_STATUS set_hba_interrupts(AhciDevice_t* device, bool value);

static ALWAYS_INLINE registers_t* ahci_irq_handler(registers_t *regs);

static ALWAYS_INLINE void* get_hba_region(AhciDevice_t* device) {

  const uint32_t bar5 = pci_read_32(&device->m_identifier->address, BAR5);
  println(to_string(bar5));

  void* hba_region = kmem_kernel_alloc(kmem_get_page_base(bar5), ALIGN_UP(sizeof(HBA), SMALL_PAGE_SIZE), KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE);

  println(to_string((uintptr_t)hba_region));

  return hba_region;
}

static ALWAYS_INLINE volatile HBA* get_hba(AhciDevice_t* device) {
  return (volatile HBA*)device->m_hba_region;
}

static ALWAYS_INLINE LIGHT_STATUS reset_hba(AhciDevice_t* device) {

  // NOTE: 0x01 is the ghc enable flag
  for (size_t retry = 0;; retry++) {

    if (retry > 1000) {
      return LIGHT_FAIL;
    }

    if (!(get_hba(device)->control_regs.global_host_ctrl & 0x1)) {
      break;
    }
    get_hba(device)->control_regs.global_host_ctrl |= 0x1;
    delay(1000);
  }

  // get this mofo up and running
  get_hba(device)->control_regs.global_host_ctrl = (1 << 31) | (1 << 1);
  get_hba(device)->control_regs.int_status = 0xffffffff;

  uint32_t pi = get_hba(device)->control_regs.ports_impl;

  for (uint32_t i = 0; i < MAX_HBA_PORT_AMOUNT; i++) {
    if (pi & (1 << i)) {
      print("HBA port ");
      print(to_string(i));
      println(" is implemented");

      volatile HBA_port_registers_t* port_regs = ((volatile HBA_port_registers_t*)&get_hba(device)->ports[i]);
      AhciPort_t* port = make_ahci_port(device, port_regs, i);
      list_append(device->m_ports, port);
      if (initialize_port(port) == LIGHT_FAIL) {
        print("Failed to initialize AHCI port ");
        println(to_string(i));
      }
    }
  }

  return LIGHT_FAIL;
}

static ALWAYS_INLINE LIGHT_STATUS initialize_hba(AhciDevice_t* device) {

  get_hba(device)->control_regs.global_host_ctrl = 0x80000000;

  // enable hba interrupts
  pci_set_interrupt_line(&device->m_identifier->address, true);
  pci_set_bus_mastering(&device->m_identifier->address, true);

  set_hba_interrupts(device, true);

  InterruptHandler_t* _handler = init_interrupt_handler(device->m_identifier->interrupt_line, I8259, ahci_irq_handler);
  add_handler(_handler);

  return reset_hba(device);
}

static ALWAYS_INLINE LIGHT_STATUS set_hba_interrupts(AhciDevice_t* device, bool value) {
  uint32_t ghc = get_hba(device)->control_regs.global_host_ctrl;
  if (value) {
    get_hba(device)->control_regs.global_host_ctrl = ghc | (1 << 1); // 0x02
  } else {
    get_hba(device)->control_regs.global_host_ctrl = ghc & 0xfffffffd;
  }
  return LIGHT_SUCCESS;
}

static ALWAYS_INLINE registers_t* ahci_irq_handler(registers_t *regs) {

  println("Recieved AHCI interrupt!");

  uint32_t ahci_interrupt_status = get_hba(s_ahci_device)->control_regs.int_status;

  if (ahci_interrupt_status == 0) {
    return regs;
  }

  for (uint32_t i = 0; i < 32; i++) {
    if (ahci_interrupt_status & (1 << i)) {
      AhciPort_t* port = list_get(s_ahci_device->m_ports, i);
      
      ahci_port_handle_int(port); 
    }
  }

  return regs;
}

// TODO:
AhciDevice_t* init_ahci_device(DeviceIdentifier_t* identifier) {
  s_ahci_device = kmalloc(sizeof(AhciDevice_t));
  s_ahci_device->m_identifier = identifier;
  s_ahci_device->m_hba_region = get_hba_region(s_ahci_device);
  s_ahci_device->m_ports = init_list();

  initialize_hba(s_ahci_device);

  return s_ahci_device;
}