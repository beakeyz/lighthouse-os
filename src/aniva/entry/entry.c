#include "entry.h"
#include "dev/core.h"
#include "dev/debug/early_tty.h"
#include "dev/disk/ahci/ahci_device.h"
#include "dev/disk/generic.h"
#include "dev/disk/ramdisk.h"
#include "dev/driver.h"
#include "dev/hid/hid.h"
#include "dev/manifest.h"
#include "dev/pci/pci.h"
#include "dev/usb/usb.h"
#include "fs/core.h"
#include "fs/namespace.h"
#include "fs/vfs.h"
#include "fs/vnode.h"
#include "kevent/kevent.h"
#include "libk/bin/ksyms.h"
#include "libk/flow/error.h"
#include "libk/data/hashmap.h"
#include "libk/io.h"
#include "libk/multiboot.h"
#include "libk/stddef.h"
#include "mem/pg.h"
#include "mem/zalloc.h"
#include "proc/core.h"
#include "proc/ipc/packet_payload.h"
#include "proc/ipc/packet_response.h"
#include "proc/ipc/thr_intrf.h"
#include "proc/kprocs/reaper.h"
#include "proc/kprocs/socket_arbiter.h"
#include "proc/proc.h"
#include "proc/socket.h"
#include "proc/thread.h"
#include "system/acpi/acpi.h"
#include "system/acpi/parser.h"
#include "system/processor/processor.h"
#include "system/processor/processor_info.h"
#include "system/resource.h"
#include "time/core.h"
#include "libk/string.h"
#include "proc/ipc/tspckt.h"
#include "intr/interrupts.h"
#include <dev/debug/serial.h>
#include <mem/heap.h>
#include <mem/kmem_manager.h>
#include <sched/scheduler.h>

#include <dev/kterm/kterm.h>

system_info_t g_system_info;

void __init _start(struct multiboot_tag *mb_addr, uint32_t mb_magic);
void kthread_entry();

driver_version_t kernel_version = DEF_DRV_VERSION(0, 0, 0);

/*
 * NOTE: has to be run after driver initialization
 */
ErrorOrPtr __init try_fetch_initramdisk() {

  struct multiboot_tag_module* mod = g_system_info.ramdisk;

  if (!mod)
    return Error();

  const paddr_t module_start = mod->mod_start;
  const paddr_t module_end = mod->mod_end;

  /* Prefetch data */
  const size_t cramdisk_size = module_end - module_start;

  /* Map user pages */
  const uintptr_t ramdisk_addr = Must(__kmem_kernel_alloc(module_start, cramdisk_size, KMEM_CUSTOMFLAG_GET_MAKE, 0));

  /* Create ramdisk object */
  disk_dev_t* ramdisk = create_generic_ramdev_at(ramdisk_addr, cramdisk_size);

  if (!ramdisk)
    return Error();

  /* We know ramdisks through modules are compressed */
  ramdisk->m_flags |= GDISKDEV_FLAG_RAM_COMPRESSED;

  /* Register the ramdisk as a disk device */
  return register_gdisk_dev(ramdisk);
}

static void register_kernel_data(paddr_t p_mb_addr) 
{
  g_system_info.sys_flags = NULL;
  g_system_info.kernel_start_addr = (vaddr_t)&_kernel_start;
  g_system_info.kernel_end_addr = (vaddr_t)&_kernel_end;
  g_system_info.kernel_version = kernel_version.version;
  g_system_info.phys_multiboot_addr = p_mb_addr;
  g_system_info.virt_multiboot_addr = kmem_ensure_high_mapping(p_mb_addr);

  /* Cache the physical address of important tags */
  g_system_info.rsdp = get_mb2_tag((void*)g_system_info.virt_multiboot_addr, MULTIBOOT_TAG_TYPE_ACPI_OLD);
  g_system_info.xsdp = get_mb2_tag((void*)g_system_info.virt_multiboot_addr, MULTIBOOT_TAG_TYPE_ACPI_NEW);
  g_system_info.firmware_fb = get_mb2_tag((void*)g_system_info.virt_multiboot_addr, MULTIBOOT_TAG_TYPE_FRAMEBUFFER);
  g_system_info.ramdisk = get_mb2_tag((void*)g_system_info.virt_multiboot_addr, MULTIBOOT_TAG_TYPE_MODULE);

  if (g_system_info.firmware_fb)
    g_system_info.sys_flags |= SYSFLAGS_HAS_FRAMEBUFFER;
}

NOINLINE void __init _start(struct multiboot_tag *mb_addr, uint32_t mb_magic) {

  /*
   * TODO: move serial to driver level, have early serial that gets migrated to a driver later 
   * TODO: create a nice logging system, with log clients that connect to a single place to post
   * logs. The logging server can collect these logs and do all sorts of stuff with them, like save
   * them to a file, display them to the current console provider, or beam them over serial or USB
   * connectors =D
   */
  init_serial();

  disable_interrupts();

  println("Hi from 64 bit land =D");
  println(to_string((uintptr_t)mb_addr));

  // Verify magic number
  if (mb_magic != 0x36d76289) {
    println("big yikes");
    println(to_string(mb_magic));
    kernel_panic("Can't verify multiboot header: invalid magic number");
  }

  // Make sure the initial system info is setup
  register_kernel_data((paddr_t)mb_addr);

  // parse multiboot
  init_multiboot((void*)g_system_info.virt_multiboot_addr);

  // init bootstrap processor
  init_processor(&g_bsp, 0);

  // bootstrap the kernel heap
  init_kheap();

  // we need memory
  init_kmem_manager((void*)g_system_info.virt_multiboot_addr);

  // Initialize an early console
  init_early_tty(g_system_info.firmware_fb);

  println("Initialized tty");

  for (;;) 
  {
    asm volatile ("hlt");
  }

  // we need more memory
  init_zalloc();

  // we need resources
  init_kresources();

  /* Initialize the ACPI subsystem */
  init_acpi();

  // initialize cpu-related things that need the memorymanager and the heap
  init_processor_late(&g_bsp);

  /* Initialize the timer system */
  init_timer_system();

  /* Initialize human interface devices */
  init_hid();

  // Initialize kevent
  init_kevents();

  /* Initialize hashmap caching */
  init_hashmap();

  /* Initialize the subsystem responsible for managing processes */
  init_proc_core();

  /* Initialize the VFS */
  init_vfs();

  /* Initialize the filesystem core */
  init_fs_core();

  //println("Did stuff");

  //for (;;) {}

  destroy_early_tty();

  /* This is where we initialize bus types */

  /* Initialize the PCI subsystem */
  init_pci();

  /* Initialize the USB subsystem */
  init_usb();

  // FIXME
  // are we going micro, mono or perhaps even exo?
  // how big will the role of the vfs be?
  //  * how will processes even interact with the kernel??? * 

  init_scheduler();

  proc_t* root_proc = create_kernel_proc(kthread_entry, NULL);

  init_socket_arbiter(root_proc);
  init_reaper(root_proc);

  set_kernel_proc(root_proc);
  sched_add_proc(root_proc);

  start_scheduler();

  // Verify not reached
  kernel_panic("Somehow came back to the kernel entry function!");
}

void kthread_entry() {

  /* Make sure the scheduler won't ruin our day */
  pause_scheduler();

  /* Initialize global disk device registers */
  init_gdisk_dev();

  /* Install and load initial drivers */
  init_aniva_driver_registry();

  /* Scan for pci devices and initialize any matching drivers */
  init_pci_drivers();

  /* Try to fetch the initrd which we can mount initial root to */
  try_fetch_initramdisk();

  /* Probe for a root device */
  init_root_device_probing();

  /* TODO:  
   *  - verify the root device
   *  - launch the userspace bootstrap
   */

  /*
   * Setup is done: we can start scheduling stuff 
   * At this point, the kernel should have created a bunch of userspace processes that are ready to run on the next schedules. Most of the 
   * 'userspace stuff' will consist of user tracking, configuration and utility processes. Any windowing will be done by the kernel this driver
   * is an external driver that we will load from the ramfs, since it's not a driver that is an absolute non-trivial piece. When we fail to load
   */
  resume_scheduler();

  for (;;) {
    asm volatile ("hlt");
  }

  kernel_panic("Reached end of start_thread");
}
