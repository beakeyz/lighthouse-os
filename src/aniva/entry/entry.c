#include "entry.h"
#include "dev/pci/pci.h"
#include "dev/framebuffer/framebuffer.h"
#include "libk/error.h"
#include "libk/kevent/core.h"
#include "libk/multiboot.h"
#include "mem/pg.h"
#include "proc/ipc/thr_intrf.h"
#include "proc/kprocs/root_process.h"
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

//typedef void (*ctor_func_t)();
//extern ctor_func_t _start_ctors[];
//extern ctor_func_t _end_ctors[];
//__attribute__((constructor)) void test() { println("[TESTCONSTRUCTOR] =D"); }

static uintptr_t first_valid_addr = 0;
static uintptr_t first_valid_alloc_addr = (uintptr_t)&_kernel_end;

void __init _start(struct multiboot_tag *mb_addr, uint32_t mb_magic);

void __init _start(struct multiboot_tag *mb_addr, uint32_t mb_magic) {

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

  init_global_kevents();

  init_timer_system();

  init_acpi(multiboot_addr);

  // NOTE: uncommented for debugging purposes
  //init_acpi_parser_aml(g_parser_ptr);
  
  init_pci();

  // TODO: ATA/NVMe/IDE support?
  //init_storage_controller();

  initialize_proc_core();

  init_aniva_driver_registry();

  init_scheduler();

  create_and_register_root_process(multiboot_addr);

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

  for (;;) {
    asm volatile ("hlt");
  }
}
