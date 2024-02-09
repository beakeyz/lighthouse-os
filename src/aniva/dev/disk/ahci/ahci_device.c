#include "ahci_device.h"
#include "dev/core.h"
#include "dev/debug/serial.h"
#include "dev/disk/ahci/ahci_port.h"
#include "dev/disk/ahci/definitions.h"
#include "dev/disk/generic.h"
#include "dev/pci/definitions.h"
#include "dev/pci/pci.h"
#include "irq/ctl/ctl.h"
#include "irq/interrupts.h"
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

static ahci_device_t* __ahci_devices = nullptr;
static aniva_driver_t* _p_base_ahci_driver = nullptr;

static pci_dev_id_t ahci_id_table[] = {
  PCI_DEVID_CLASSES(MASS_STORAGE, PCI_SUBCLASS_SATA, PCI_PROGIF_SATA),
  PCI_DEVID_END
};

int ahci_driver_init();
int ahci_driver_exit();

static ALWAYS_INLINE void* get_hba_region(ahci_device_t* device);
static ALWAYS_INLINE ANIVA_STATUS reset_hba(ahci_device_t* device);
static ALWAYS_INLINE ANIVA_STATUS initialize_hba(ahci_device_t* device);

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


/*!
 * @brief Reset the HBA hardware and scan for ports
 *
 */
static ANIVA_STATUS reset_hba(ahci_device_t* device) {

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
      register_gdisk_dev(port->m_generic);

      list_append(device->m_ports, port);

      internal_index++;
    }
  }

  return ANIVA_SUCCESS;
}

static ErrorOrPtr gather_ports_info(ahci_device_t* device) {

  ANIVA_STATUS status;

  FOREACH(i, device->m_ports) {

    ahci_port_t* port = i->data;

    status = ahci_port_gather_info(port);

    if (status == ANIVA_FAIL) {
      println("Failed to gather info!");
      return Error();
    }
  }

  return Success(0);
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
      udelay(100);
    }
  }

  ANIVA_STATUS status = reset_hba(device);

  if (status != ANIVA_SUCCESS)
    return status;

  // HBA has been reset, enable its interrupts and claim this line
  /* TODO: notify PCI of any interrupt line changes */
  int error = irq_allocate(interrupt_line, NULL, ahci_irq_handler, NULL, "AHCI controller");

  /* TODO: handle this propperly =)))) */
  ASSERT_MSG(error == NULL, "Failed to allocate an IRQ for this AHCI device!");

  uint32_t ghc = ahci_mmio_read32((uintptr_t)device->m_hba_region, AHCI_REG_AHCI_GHC) | AHCI_GHC_IE;
  ahci_mmio_write32((uintptr_t)device->m_hba_region, AHCI_REG_AHCI_GHC, ghc);

  println("Gathering info about ports...");

  if (IsError(gather_ports_info(device)))
      return ANIVA_FAIL;

  println("Done!");
  return status;
}

static registers_t* ahci_irq_handler(registers_t *regs) {

  ahci_device_t* device = __ahci_devices;

  // TODO: handle
  //kernel_panic("Recieved AHCI interrupt!");

  while (device) {
  
    uint32_t ahci_interrupt_status = 0;

    ahci_mmio_read32((uintptr_t)device->m_hba_region, AHCI_REG_IS);

    if (ahci_interrupt_status == 0)
      goto next;

    FOREACH(i, device->m_ports) {
      ahci_port_t* port = i->data;

      /* Sanity check */
      ASSERT(port->m_port_index < 32);

      if (ahci_interrupt_status & (1 << port->m_port_index)) {
        ahci_port_handle_int(port); 
      }
    }

next:
    device = device->m_next;
  }

  return regs;
}

// TODO:
ahci_device_t* create_ahci_device(pci_device_t* identifier) {
  ahci_device_t* ahci_device = kmalloc(sizeof(ahci_device_t));

  memset(ahci_device, 0, sizeof(ahci_device_t));

  __register_ahci_device(ahci_device);

  ahci_device->m_identifier = identifier;
  ahci_device->m_ports = init_list();
  ahci_device->m_parent = _p_base_ahci_driver;

  if (initialize_hba(ahci_device) == ANIVA_FAIL) {
    destroy_ahci_device(ahci_device);
    return nullptr;
  }

  return ahci_device;
}

void destroy_ahci_device(ahci_device_t* device) {

  __unregister_ahci_device(device);

  FOREACH(i, device->m_ports) {
    destroy_ahci_port(i->data);
  }

  destroy_list(device->m_ports);

  kfree(device);
}

static int ahci_probe(pci_device_t* device, pci_driver_t* driver) {

  /* Enable the device first */
  pci_device_enable(device);

  ahci_device_t* ahci_device = create_ahci_device(device);

  if (!ahci_device)
    return -1;

  return 0;
}

pci_driver_t ahci_pci_driver = {
  .id_table = ahci_id_table,
  .f_probe = ahci_probe,
  .device_flags = NULL,
};


uintptr_t ahci_driver_on_packet(aniva_driver_t* this, dcc_t code, void* buffer, size_t size, void* out_buffer, size_t out_size) {

  if (__ahci_devices == nullptr) {
    return (uintptr_t)-1;
  }

  kernel_panic("TODO: implement ahci on_packet functions");

  return 0;
}

/*
 * Drv/disk/ahci
 */
aniva_driver_t base_ahci_driver = {
  .m_name = "ahci",
  .m_type = DT_DISK,
  .m_version = DRIVER_VERSION(0, 0, 1),
  .f_init = ahci_driver_init,
  .f_exit = ahci_driver_exit,
  .f_msg = ahci_driver_on_packet,
  /*
   * FIXME: when we insert a dependency here that is deferred (a socket), we completely die, since this 
   * non-socket driver will fail to load as it realizes that it is unable to load its dependency...
   */
  .m_dep_count = 0
};
EXPORT_DRIVER_PTR(base_ahci_driver);

/*!
 * @brief: Initialize the AHCI bus+device driver
 *
 * TODO: refactor filenames to something logical lmao
 *       ahci_device -> ahci_bus
 *       ahci_port -> ahci_device (Something like this?)
 *
 * Here we register our PCI driver (So we are in line to recieve the ACHI controllers PCI device once the 
 * PCI bus driver is initialized) and we register our own bus group. The group will simply be called 'ahci' which is
 * where all the ahci controllers and devices we find will be attached.
 */
int ahci_driver_init() 
{
  __ahci_devices = nullptr;
  _p_base_ahci_driver = &base_ahci_driver;

  register_pci_driver(&ahci_pci_driver);

  register_bus_group("ahci");

  return 0;
}

int ahci_driver_exit() 
{
  // shutdown ahci device
  println("Shut down ahci driver");

  _p_base_ahci_driver = nullptr;

  if (__ahci_devices) {
    destroy_ahci_device(__ahci_devices);
  }

  unregister_pci_driver(&ahci_pci_driver);

  return 0;
}
