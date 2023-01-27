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
#include "time/core.h"
#include "libk/queue.h"
#include "proc/ipc/tspckt.h"
#include "proc/socket.h"
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
static uintptr_t first_valid_alloc_addr = (uintptr_t)&_kernel_end;

GlobalSystemInfo_t g_GlobalSystemInfo;

__attribute__((constructor)) void test() { println("[TESTCONSTRUCTOR] =D"); }
void test_sender_thread();

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

  // NOTE: testhandler
  InterruptHandler_t *test = init_interrupt_handler(1, I8259, thing);
  test->m_controller->fControllerEnableVector(1);
  add_handler(test);

  init_timer_system();

  init_fb(get_mb2_tag((uintptr_t *)mb_addr, MULTIBOOT_TAG_TYPE_FRAMEBUFFER));
  // Next TODO: create a kernel pre-init tty

  init_acpi();

  init_pci();

  // TODO: ATA/NVMe/IDE support?
  init_storage_controller();

  initialize_proc_core();
  init_scheduler();

  proc_t *aniva_proc = create_clean_proc("aniva_core", 0);
  proc_add_thread(aniva_proc, create_thread_as_socket(aniva_proc, aniva_task, "aniva_socket", 0));
  proc_add_thread(aniva_proc, create_thread_for_proc(aniva_proc, test_sender_thread, NULL, "test_sender"));

  sched_add_proc(aniva_proc);

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

void test_sender_thread() {

  uintptr_t data = 696969;
  uintptr_t data_2 = 696970;

  size_t data_size = sizeof(uintptr_t);

  tspckt_t **response_ptr = (tspckt_t**)Must(send_packet_to_socket(0, &data, data_size));
  Must(send_packet_to_socket(0, &data_2, data_size));

  println("---------- sent the thing");
  for (;;) {
    tspckt_t *response = *response_ptr;

    if (response && validate_tspckt(response)) {

      // legitimacy
      print("Got data: ");
      println(to_string(*(uintptr_t*)response->m_data));
      kernel_panic("Got response!");
    }
  }
}

__attribute__((noreturn)) void aniva_task(queue_t* buffer) {

  uintptr_t response_data = 707070;

  for (;;) {
    tspckt_t* packet = queue_dequeue(buffer);
    if (packet) {
      uintptr_t data = *(uintptr_t*)packet->m_data;
      print("Received packet! data: ");
      println(to_string(data));

      packet->m_response = create_tspckt(&response_data, sizeof(uintptr_t));
    } else {
      //print("n");
    }
  }
}
