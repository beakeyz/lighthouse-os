#include "root_process.h"
#include "dev/core.h"
#include "dev/debug/serial.h"
#include "dev/debug/test.h"
#include "dev/disk/ahci/ahci_device.h"
#include "dev/framebuffer/framebuffer.h"
#include "dev/keyboard/ps2_keyboard.h"
#include "dev/kterm/kterm.h"
#include "interupts/interupts.h"
#include "libk/async_ptr.h"
#include "libk/error.h"
#include "libk/io.h"
#include "libk/linkedlist.h"
#include "libk/multiboot.h"
#include "libk/queue.h"
#include "libk/string.h"
#include "mem/heap.h"
#include "mem/kmem_manager.h"
#include "proc/core.h"
#include "proc/ipc/packet_response.h"
#include "proc/ipc/tspckt.h"
#include "proc/proc.h"
#include "proc/socket.h"
#include "proc/thread.h"
#include "sched/scheduler.h"
#include "sync/mutex.h"
#include "sync/spinlock.h"
#include "system/acpi/acpi.h"
#include "system/acpi/parser.h"
#include "system/processor/processor.h"
#include <dev/manifest.h>

static void root_main(uintptr_t multiboot_address);
static void root_packet_dispatch();

void create_and_register_root_process(uintptr_t multiboot_address) {

  proc_t* root_proc = create_kernel_proc(root_main, multiboot_address);
  set_kernel_proc(root_proc);

  // base drivers get hooked on the main processor and its kernel process

  proc_add_thread(root_proc, create_thread_for_proc(root_proc, root_packet_dispatch, NULL, "root packet dispatch"));

  sched_add_proc(root_proc);

}

static void root_main(uintptr_t multiboot_address) {

  Must(load_driver(create_dev_manifest((aniva_driver_t*)&g_test_dbg_driver, 0)));
  Must(load_driver(create_dev_manifest((aniva_driver_t*)&g_base_ps2_keyboard_driver, 0)));
  Must(load_driver(create_dev_manifest((aniva_driver_t*)&g_base_ahci_driver, 0)));

  if (multiboot_address) {
    // load the kterm driver, which also loads the fb driver
    Must(load_driver(create_dev_manifest((aniva_driver_t*)&g_base_fb_driver, 0)));

    destroy_packet_response(await(driver_send_packet("diagnostics.debug", 1, NULL, 0)));

    destroy_packet_response(await(driver_send_packet("graphics.fb", FB_DRV_SET_MB_TAG, get_mb2_tag((void*)multiboot_address, MULTIBOOT_TAG_TYPE_FRAMEBUFFER), sizeof(struct multiboot_tag_framebuffer))));

    Must(load_driver(create_dev_manifest((aniva_driver_t*)&g_base_kterm_driver, 0)));
  }
}

static void root_packet_dispatch() {

  for (;;) {

    Processor_t* current = get_current_processor();
    processor_increment_critical_depth(current);

    tspckt_t* packet;

    while ((packet = queue_peek(current->m_packet_queue.m_packets)) != nullptr) {
      ErrorOrPtr result = socket_handle_tspacket(packet);

      queue_dequeue(current->m_packet_queue.m_packets);
      if (result.m_status == ANIVA_WARNING) {
        // packet was not ready, delay
        queue_enqueue(current->m_packet_queue.m_packets, packet);
        break;
      }
    }

    processor_decrement_critical_depth(current);

    // after one swoop we don't need to check again lmao
    scheduler_yield();
  }

  kernel_panic("TRIED TO EXIT root_packet_dispatch!");
}
