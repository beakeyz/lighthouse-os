#include <kmain.h>
#include "dev/disk/storage_controller.h"
#include "dev/pci/pci.h"
#include "interupts/control/interrupt_control.h"
#include "dev/framebuffer/framebuffer.h"
#include "libk/bitmap.h"
#include "libk/error.h"
#include "libk/kevent/core.h"
#include "libk/kevent/eventhook.h"
#include "libk/kevent/eventregister.h"
#include "libk/linkedlist.h"
#include "sync/atomic_ptr.h"
#include "system/acpi/acpi.h"
#include "system/acpi/parser.h"
#include "system/acpi/structures.h"
#include "system/asm_specifics.h"
#include "system/processor/processor.h"
#include "time/pit.h"
#include <dev/debug/serial.h>
#include <interupts/control/pic.h>
#include <interupts/idt.h>
#include <interupts/interupts.h>
#include <mem/kmalloc.h>
#include <mem/kmem_manager.h>
#include <libk/multiboot.h>
#include <sched/scheduler.h>
#include <libk/io.h>
#include <libk/stddef.h>
#include <libk/string.h>

typedef void (*ctor_func_t)();

extern ctor_func_t _start_ctors[];
extern ctor_func_t _end_ctors[];

static uintptr_t first_valid_addr = 0;
static uintptr_t first_valid_alloc_addr = (uintptr_t) &_kernel_end;

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

  init_kheap(); // FIXME: this heap impl is very bad. improve it

  g_GlobalSystemInfo.m_bsp_processor.fLateInit(&g_GlobalSystemInfo.m_bsp_processor);

  init_kmem_manager((uintptr_t *)
                      mb_addr, first_valid_addr, first_valid_alloc_addr);
  g_GlobalSystemInfo.m_multiboot_addr = (uintptr_t)
    kmem_kernel_alloc((uintptr_t)
                        mb_addr, g_GlobalSystemInfo.m_total_multiboot_size + 8, KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE);
  struct multiboot_tag_framebuffer *fb = get_mb2_tag((uintptr_t *)
                                                       mb_addr, MULTIBOOT_TAG_TYPE_FRAMEBUFFER);

  // NOTE: testhandler
  InterruptHandler_t *test = init_interrupt_handler(1, I8259, thing);
  test->m_controller->fControllerEnableVector(1);
  add_handler(test);

  init_and_install_pit();

  init_fb(fb);
  // Next TODO: create a kernel pre-init tty

  init_acpi();

  init_pci();

  // TODO: ATA/NVMe/IDE support?
  init_storage_controller();

  init_global_kevents();

  init_scheduler();

  enable_interrupts();

  start_scheduler();

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
