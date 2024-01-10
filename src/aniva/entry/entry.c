#include "entry.h"
#include "dev/core.h"
#include "dev/debug/early_tty.h"
#include "dev/disk/ahci/ahci_device.h"
#include "dev/disk/generic.h"
#include "dev/disk/ramdisk.h"
#include "dev/driver.h"
#include "dev/external.h"
#include "dev/io/hid/hid.h"
#include "dev/loader.h"
#include "dev/manifest.h"
#include "dev/pci/pci.h"
#include "dev/usb/usb.h"
#include "fs/core.h"
#include "fs/namespace.h"
#include "fs/vfs.h"
#include "fs/vnode.h"
#include <fs/device/core.h>
#include "kevent/event.h"
#include "kevent/hook.h"
#include "libk/bin/ksyms.h"
#include "libk/flow/error.h"
#include "libk/data/hashmap.h"
#include "libk/io.h"
#include "libk/multiboot.h"
#include "libk/stddef.h"
#include "logging/log.h"
#include "mem/pg.h"
#include "mem/zalloc.h"
#include "proc/core.h"
#include "proc/ipc/packet_payload.h"
#include "proc/ipc/packet_response.h"
#include "proc/ipc/thr_intrf.h"
#include "proc/kprocs/reaper.h"
#include "proc/kprocs/socket_arbiter.h"
#include "proc/proc.h"
#include "proc/profile/profile.h"
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
#include "irq/interrupts.h"
#include <dev/debug/serial.h>
#include <mem/heap.h>
#include <mem/kmem_manager.h>
#include <sched/scheduler.h>

system_info_t g_system_info;

void __init _start(struct multiboot_tag *mb_addr, uint32_t mb_magic);
void kthread_entry();

driver_version_t kernel_version = DEF_DRV_VERSION(0, 1, 0);

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

NOINLINE void __init _start(struct multiboot_tag *mb_addr, uint32_t mb_magic) 
{
  /*
   * TODO: move serial to driver level, have early serial that gets migrated to a driver later 
   * TODO: create a nice logging system, with log clients that connect to a single place to post
   * logs. The logging server can collect these logs and do all sorts of stuff with them, like save
   * them to a file, display them to the current console provider, or beam them over serial or USB
   * connectors =D
   */
  disable_interrupts();

  proc_t* root_proc;

  // Logging system asap
  init_early_logging();

  // First logger
  init_serial();

  printf("Hi from within (%s)\n", "Aniva");
  printf("Multiboot address from the bootloader is at: %s\n", to_string((uintptr_t)mb_addr));

  // Verify magic number
  if (mb_magic != MULTIBOOT2_BOOTLOADER_MAGIC) {
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

  /*
   * bootstrap the kernel heap
   * We're pretty nicely under way to be able to move this call
   * under init_kmem_manager, since we almost don't need the heap 
   * in that system anymore. When that happens, we are able to completely
   * initialize the kheap off of kmem_manager, since we can just 
   * ask it for bulk memory that we can then create a memory_allocator
   * from
   */
  init_kheap();

  // we need memory
  init_kmem_manager((void*)g_system_info.virt_multiboot_addr);

  // Initialize logging right after the memory setup
  init_logging();

  // Initialize an early console
  //init_early_tty(g_system_info.firmware_fb);

  println("Initialized tty");

  // we need more memory
  init_zalloc();

  // we need resources
  init_kresources();

  // Setup interrupts (Fault handlers and IRQ request framework)
  init_interrupts();

  /* Initialize the ACPI subsystem */
  init_acpi();

  println("[X] Processor...");
  // initialize cpu-related things that need the memorymanager and the heap
  init_processor_late(&g_bsp);

  println("[X] Timer...");
  /* Initialize the timer system */
  init_timer_system();

  println("[X] hashmap...");
  /* Initialize hashmap caching */
  init_hashmap();

  println("[X] kevents...");
  // Initialize kevent
  init_kevents();

  println("[X] proc core...");
  /* Initialize the subsystem responsible for managing processes */
  init_proc_core();

  println("[X] vfs...");
  /* Initialize the VFS */
  init_vfs();

  println("[X] fs core...");
  /* Initialize the filesystem core */
  init_fs_core();

  /* Make sure device access is initialized */
  init_dev_access();

  /* TMP: remove early dbg tty */
  destroy_early_tty();

  /* Initialize HID driver subsystem */
  init_hid();

  /* Initialize the PCI subsystem */
  init_pci();

  /* Initialize the USB subsystem */
  init_usb();

  // FIXME
  // are we going micro, mono or perhaps even exo?
  // how big will the role of the vfs be?
  //  * how will processes even interact with the kernel??? * 

  /* Initialize scheduler on the bsp */
  init_scheduler(0);

  root_proc = create_kernel_proc(kthread_entry, NULL);

  ASSERT_MSG(root_proc, "Failed to create a root process!");

  /* Create a reaper thread to kill processes through an async pipeline */
  init_reaper(root_proc);

  /* Register our kernel process */
  set_kernel_proc(root_proc);

  /* Add it to the scheduler */
  sched_add_proc(root_proc);

  /* Start the scheduler (Should never return) */
  start_scheduler();

  // Verify not reached
  kernel_panic("Somehow came back to the kernel entry function!");
}

/*!
 * @brief: Our kernel thread entry point
 *
 * This function is not supposed to be called explicitly, but rather it serves as the 
 * 'third stage' of our kernel, with stage one being our assembly setup and stage two 
 * being the start of our C code.
 */
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

  /* Do late initialization of the default profiles */
  init_profiles_late();

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

  extern_driver_t* kterm = load_external_driver("Root/System/kterm.drv");

  ASSERT_MSG(kterm, "Failed to load kterm!");

  while (true) {
    scheduler_yield();
  }

  /* Should not happen lmao */
  kernel_panic("Reached end of start_thread");
}
