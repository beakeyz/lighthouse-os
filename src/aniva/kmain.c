#include <kmain.h>
#include "dev/disk/storage_controller.h"
#include "dev/pci/pci.h"
#include "interupts/control/interrupt_control.h"
#include "dev/framebuffer/framebuffer.h"
#include "libk/error.h"
#include "libk/kevent/core.h"
#include "proc/ipc/thr_intrf.h"
#include "system/acpi/acpi.h"
#include "system/processor/processor.h"
#include "time/core.h"
#include "proc/socket.h"
#include "libk/string.h"
#include "proc/ipc/tspckt.h"
#include "interupts/interupts.h"
#include "dev/driver.h"
#include "mem/heap.h"
#include "mem/kmalloc.h"
#include "mem/zalloc.h"
#include <dev/debug/serial.h>
#include <mem/heap.h>
#include <mem/kmem_manager.h>
#include <sched/scheduler.h>
#include <libk/io.h>

typedef void (*ctor_func_t)();

extern ctor_func_t _start_ctors[];
extern ctor_func_t _end_ctors[];

static uintptr_t first_valid_addr = 0;
static uintptr_t first_valid_alloc_addr = (uintptr_t)&_kernel_end;

GlobalSystemInfo_t g_GlobalSystemInfo;

__attribute__((constructor)) void test() { println("[TESTCONSTRUCTOR] =D"); }

registers_t *thing(registers_t *regs) {
  in8(0x60);
  println("funnie");
  return regs;
}

void _start(struct multiboot_tag *mb_addr, uint32_t mb_magic) {

  init_serial();
  println("Hi from 64 bit land =D");

  // Verify magic number
  if (mb_magic != 0x36d76289) {
    println("big yikes");
    kernel_panic("Can't verify multiboot header: invalid magic number");
  }

  // parse multiboot
  mb_initialize((void *) mb_addr, &first_valid_addr, &first_valid_alloc_addr);
  g_GlobalSystemInfo.m_total_multiboot_size = get_total_mb2_size((void *) mb_addr);

  // init bootstrap processor
  init_processor(&g_GlobalSystemInfo.m_bsp_processor, 0);

  // bootstrap the kernel heap
  init_kheap();

  // initialize things like fpu or interrupts
  g_GlobalSystemInfo.m_bsp_processor.fLateInit(&g_GlobalSystemInfo.m_bsp_processor);

  // we need memory
  init_kmem_manager((uintptr_t *)mb_addr, first_valid_addr, first_valid_alloc_addr);

  // map multiboot address
  g_GlobalSystemInfo.m_multiboot_addr = (uintptr_t)kmem_kernel_alloc(
    (uintptr_t)mb_addr,
    g_GlobalSystemInfo.m_total_multiboot_size + 8,
    KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE
  );

  init_global_kevents();

  init_timer_system();

  init_fb(get_mb2_tag((uintptr_t *)mb_addr, MULTIBOOT_TAG_TYPE_FRAMEBUFFER));

  init_acpi();
  
  init_pci();

  // TODO: ATA/NVMe/IDE support?
  init_storage_controller();

  initialize_proc_core();
  init_aniva_driver_register();

  init_scheduler();

  proc_t *aniva_proc = create_clean_proc("aniva_core", 0);
  proc_add_thread(aniva_proc, create_thread_as_socket(aniva_proc, aniva_task, "aniva_socket", 0));

  sched_add_proc(aniva_proc);

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

void init(){
  println("   -   hello from the test driver");
}
int exit(){
  println("   -   goodbye from the test driver");
  return 0;
}
int ioctl(char* fmt, ...){
  println("   -   called the driver message routine");
  return 0;
}

void aniva_task(queue_t *buffer) {
  println("creating new test heap");

  const vaddr_t new_heap_vbase = 0xffffffff60000000UL;
  zone_allocator_t* new_heap = create_zone_allocator(15 * Kib, new_heap_vbase, GHEAP_KERNEL);

  void* allocation1 = new_heap->m_heap->f_allocate(new_heap, 10);
  void* allocation2 = new_heap->m_heap->f_allocate(new_heap, 10);

  *(uintptr_t*)allocation1 = 6969;

  println(to_string((uintptr_t)allocation1));
  println(to_string(*(uintptr_t*)allocation1));

  println("------------------");
  *(uintptr_t*)allocation2 = 420420;

  println(to_string((uintptr_t)allocation2));
  println(to_string(*(uintptr_t*)allocation2));

  new_heap->m_heap->f_sized_deallocate(new_heap, allocation1, 10);

  println("------------------");
  println(to_string((uintptr_t)allocation1));
  println(to_string(*(uintptr_t*)allocation1));
  void* allocation3 = new_heap->m_heap->f_allocate(new_heap, 10);
  *(uintptr_t*)allocation3 = 6979;
  println("------------------");
  println(to_string((uintptr_t)allocation3));
  println(to_string(*(uintptr_t*)allocation3));
}
