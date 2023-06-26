#include "entry.h"
#include "dev/disk/ahci/ahci_device.h"
#include "dev/disk/generic.h"
#include "dev/disk/ramdisk.h"
#include "dev/pci/pci.h"
#include "dev/framebuffer/framebuffer.h"
#include "fs/core.h"
#include "fs/namespace.h"
#include "fs/vfs.h"
#include "fs/vnode.h"
#include "kevent/kevent.h"
#include "libk/async_ptr.h"
#include "libk/error.h"
#include "libk/hashmap.h"
#include "libk/io.h"
#include "libk/multiboot.h"
#include "libk/stddef.h"
#include "mem/pg.h"
#include "proc/core.h"
#include "proc/ipc/packet_response.h"
#include "proc/ipc/thr_intrf.h"
#include "proc/kprocs/reaper.h"
#include "proc/proc.h"
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

//typedef void (*ctor_func_t)();
//extern ctor_func_t _start_ctors[];
//extern ctor_func_t _end_ctors[];
//__attribute__((constructor)) void test() { println("[TESTCONSTRUCTOR] =D"); }

/*
 * NOTE: has to be run after driver initialization
 */
ErrorOrPtr __init try_fetch_initramdisk(uintptr_t multiboot_addr) {
  println("Looking for ramdisk...");

  struct multiboot_tag_module* mod = get_mb2_tag((void*)multiboot_addr, MULTIBOOT_TAG_TYPE_MODULE);

  if (!mod)
    return Error();

  const paddr_t module_start = mod->mod_start;
  const paddr_t module_end = mod->mod_end;

  /* Prefetch data */
  const size_t cramdisk_size = module_end - module_start;

  /* Map user pages */
  const uintptr_t ramdisk_addr = Must(__kmem_kernel_alloc(module_start, cramdisk_size, KMEM_CUSTOMFLAG_GET_MAKE, 0));

  /* Create ramdisk object */
  generic_disk_dev_t* ramdisk = create_generic_ramdev_at(ramdisk_addr, cramdisk_size);

  if (!ramdisk)
    return Error();

  /* We know ramdisks through modules are compressed */
  ramdisk->m_flags |= GDISKDEV_RAM_COMPRESSED;

  /* Register the ramdisk as a disk device */
  return register_gdisk_dev(ramdisk);
}

NOINLINE void __init _start(struct multiboot_tag *mb_addr, uint32_t mb_magic) {

  /* TODO: move serial to driver level, have early serial that gets migrated to a driver later */
  init_serial();
  println("Hi from 64 bit land =D");

  /* FIXME: do we still need this? */
  void* virtual_mb_addr = (void*)kmem_ensure_high_mapping((uintptr_t)mb_addr);

  // Verify magic number
  if (mb_magic != 0x36d76289) {
    println("big yikes");
    println(to_string(mb_magic));
    kernel_panic("Can't verify multiboot header: invalid magic number");
  }

  // parse multiboot
  init_multiboot(virtual_mb_addr);
  size_t total_multiboot_size = get_total_mb2_size((void *) mb_addr);

  // init bootstrap processor
  init_processor(&g_bsp, 0);

  // bootstrap the kernel heap
  init_kheap();

  // we need memory
  init_kmem_manager((uintptr_t*)virtual_mb_addr);

  // we need resources
  init_kresources();

  // initialize cpu-related things that need the memorymanager and the heap
  g_bsp.fLateInit(&g_bsp);

  // map multiboot address
  const size_t final_multiboot_size = total_multiboot_size + 8;
  const uintptr_t multiboot_addr = (uintptr_t)Must(__kmem_kernel_alloc(
    (uintptr_t)mb_addr,
    final_multiboot_size,
    NULL,
    NULL
  ));

  g_system_info.multiboot_addr = multiboot_addr;
  g_system_info.total_multiboot_size = final_multiboot_size;
  g_system_info.has_framebuffer = (get_mb2_tag((void*)g_system_info.multiboot_addr, MULTIBOOT_TAG_TYPE_FRAMEBUFFER) != nullptr);

  // Initialize kevent
  init_kevents();

  init_hashmap();

  init_timer_system();

  init_acpi(multiboot_addr);

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

/*
 * TODO: remove test proc
 */
void test_proc_entry(uintptr_t arg) {

  println("Tried to do funnie");
  println_kterm("Hello from the test process");

  uintptr_t thing = 5;

  /* Let's attempt to terminate ourselves */
  proc_exit();

  for (;;) {
    asm volatile("hlt");
  }
}

void kthread_entry() {

  CHECK_AND_DO_DISABLE_INTERRUPTS();


  init_gdisk_dev();

  init_aniva_driver_registry();

  try_fetch_initramdisk((uintptr_t)g_system_info.multiboot_addr);

  init_root_device_probing();

  //proc_t* test_proc = create_proc("Test", test_proc_entry, NULL, PROC_KERNEL);

  //sched_add_proc(test_proc);

  CHECK_AND_TRY_ENABLE_INTERRUPTS();

  for (;;) {
    asm volatile ("hlt");
  }

  kernel_panic("Reached end of start_thread");
}
