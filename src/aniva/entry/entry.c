#include "entry.h"
#include "dev/disk/ahci/ahci_device.h"
#include "dev/pci/pci.h"
#include "dev/framebuffer/framebuffer.h"
#include "fs/core.h"
#include "fs/namespace.h"
#include "fs/vfs.h"
#include "libk/error.h"
#include "libk/multiboot.h"
#include "libk/stddef.h"
#include "mem/pg.h"
#include "proc/ipc/thr_intrf.h"
#include "system/acpi/acpi.h"
#include "system/acpi/parser.h"
#include "system/processor/processor.h"
#include "system/processor/processor_info.h"
#include "time/core.h"
#include "libk/string.h"
#include "proc/ipc/tspckt.h"
#include "interupts/interupts.h"
#include <dev/debug/serial.h>
#include <mem/heap.h>
#include <mem/kmem_manager.h>
#include <sched/scheduler.h>

system_info_t g_system_info;

void __init _start(struct multiboot_tag *mb_addr, uint32_t mb_magic);
NORETURN void kernel_thread();

//typedef void (*ctor_func_t)();
//extern ctor_func_t _start_ctors[];
//extern ctor_func_t _end_ctors[];
//__attribute__((constructor)) void test() { println("[TESTCONSTRUCTOR] =D"); }

NOINLINE void __init try_fetch_initramdisk(uintptr_t multiboot_addr) {
  println("Looking for ramdisk...");

  struct multiboot_tag_module* mod = get_mb2_tag((void*)multiboot_addr, MULTIBOOT_TAG_TYPE_MODULE);

  while (mod) {
    const uintptr_t module_start = mod->mod_start;
    const uintptr_t module_end = mod->mod_end;

    println("Found module!");
    println(to_string(module_start));
    println(to_string(module_end));

    mod = next_mb2_tag(mod + mod->size, MULTIBOOT_TAG_TYPE_MODULE);
  }
}

static uintptr_t first_valid_addr = 0;
static uintptr_t first_valid_alloc_addr = (uintptr_t)&_kernel_end;

NOINLINE void __init _start(struct multiboot_tag *mb_addr, uint32_t mb_magic) {

  init_serial();
  println("Hi from 64 bit land =D");

  // Verify magic number
  if (mb_magic != 0x36d76289) {
    println("big yikes");
    kernel_panic("Can't verify multiboot header: invalid magic number");
  }

  // parse multiboot
  mb_initialize((void *) mb_addr, &first_valid_addr, &first_valid_alloc_addr);
  size_t total_multiboot_size = get_total_mb2_size((void *) mb_addr);

  // init bootstrap processor
  init_processor(&g_bsp, 0);

  // bootstrap the kernel heap
  init_kheap();

  // initialize things like fpu or interrupts
  g_bsp.fLateInit(&g_bsp);

  // we need memory
  init_kmem_manager((uintptr_t *)mb_addr, first_valid_addr, first_valid_alloc_addr);

  // map multiboot address
  uintptr_t multiboot_addr = (uintptr_t)kmem_kernel_alloc(
    (uintptr_t)mb_addr,
    total_multiboot_size + 8,
    KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE | KMEM_CUSTOMFLAG_IDENTITY
  );

  try_fetch_initramdisk((uintptr_t)mb_addr);

  g_system_info.multiboot_addr = multiboot_addr;
  g_system_info.total_multiboot_size = total_multiboot_size + 8;

  //init_global_kevents();

  init_timer_system();

  init_acpi(multiboot_addr);
  
  init_pci();

  initialize_proc_core();

  init_vfs();

  init_fs_core();

  // NOTE: test
  // TODO: how the actual fuck do we want to manage devices in our kernel???
  // are we going micro, mono or perhaps even exo?
  // how big will the role of the vfs be?
  //  * how will processes even interact with the kernel??? * 
  Must(vfs_attach_namespace("l_dev"));
  Must(vfs_attach_namespace("l_dev/graphics"));
  Must(vfs_attach_namespace("l_dev/io"));
  Must(vfs_attach_namespace("l_dev/disk"));

  init_scheduler();

  proc_t* root_proc = create_kernel_proc(kernel_thread, NULL);
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

NORETURN void kernel_thread() {

  init_aniva_driver_registry();

  for (;;) {
    asm volatile ("hlt");
  }

  kernel_panic("Reached end of start_thread");
}
