#include "entry.h"
#include "dev/core.h"
#include "dev/disk/ahci/ahci_device.h"
#include "dev/disk/generic.h"
#include "dev/disk/ramdisk.h"
#include "dev/driver.h"
#include "dev/manifest.h"
#include "dev/pci/pci.h"
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
#include "interrupts/interrupts.h"
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

  /* Cache important tags */
  g_system_info.rsdp = get_mb2_tag((void*)g_system_info.virt_multiboot_addr, MULTIBOOT_TAG_TYPE_ACPI_OLD);
  g_system_info.xsdp = get_mb2_tag((void*)g_system_info.virt_multiboot_addr, MULTIBOOT_TAG_TYPE_ACPI_NEW);
  g_system_info.firmware_fb = get_mb2_tag((void*)g_system_info.virt_multiboot_addr, MULTIBOOT_TAG_TYPE_FRAMEBUFFER);
  g_system_info.ramdisk = get_mb2_tag((void*)g_system_info.virt_multiboot_addr, MULTIBOOT_TAG_TYPE_MODULE);

  if (g_system_info.firmware_fb)
    g_system_info.sys_flags |= SYSFLAGS_HAS_FRAMEBUFFER;
}

NOINLINE void __init _start(struct multiboot_tag *mb_addr, uint32_t mb_magic) {

  /* TODO: move serial to driver level, have early serial that gets migrated to a driver later */
  init_serial();
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

  // we need more memory
  init_zalloc();

  // we need resources
  init_kresources();

  // initialize cpu-related things that need the memorymanager and the heap
  g_bsp.fLateInit(&g_bsp);

  // Initialize kevent
  init_kevents();

  init_hashmap();

  init_timer_system();

  init_acpi();

  init_pci();

  init_proc_core();

  init_vfs();

  init_fs_core();

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

  //enable_interrupts();
  // For a context switch:
  //  - discard kernel boot stack
  //  - make new proc context
  //  - initialize its idle thread context
  //  - setup exit mechanism, which cleans up the thread and process structures and deps
  //  - since kernel idle thread SHOULD never exit, this is just as fallback. any other thread
  //    should also have an exit trap
  //  - when we are able to schedule a new thread, save current threads info into the new threads structure, then
  //    set up a new thread context and jump to the thread entry (or have a unified ep for threads which setup things
  //    in their own context
  //  - have this mechanism in place in such a way that when a thread is done and exists, it automatically cleans up
  //  - the stack and reverts to its sub-thread if it has it.
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

  /* Setup is done: we can start scheduling stuff */
  resume_scheduler();

  for (;;) {
    asm volatile ("hlt");
  }

  kernel_panic("Reached end of start_thread");
}
