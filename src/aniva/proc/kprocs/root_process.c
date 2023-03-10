#include "root_process.h"
#include "dev/core.h"
#include "dev/debug/serial.h"
#include "dev/debug/test.h"
#include "dev/framebuffer/framebuffer.h"
#include "dev/keyboard/ps2_keyboard.h"
#include "dev/kterm/kterm.h"
#include "interupts/interupts.h"
#include "libk/error.h"
#include "libk/io.h"
#include "libk/linkedlist.h"
#include "libk/multiboot.h"
#include "libk/string.h"
#include "mem/heap.h"
#include "proc/core.h"
#include "proc/ipc/packet_response.h"
#include "proc/ipc/tspckt.h"
#include "proc/proc.h"
#include "proc/socket.h"
#include "proc/thread.h"
#include "sched/scheduler.h"
#include "sync/mutex.h"
#include "sync/spinlock.h"
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

  if (multiboot_address) {
    // load the kterm driver, which also loads the fb driver
    Must(load_driver(create_dev_manifest((aniva_driver_t*)&g_base_fb_driver, 0)));

    driver_send_packet("graphics.fb", FB_DRV_SET_MB_TAG, get_mb2_tag((void*)multiboot_address, MULTIBOOT_TAG_TYPE_FRAMEBUFFER), sizeof(struct multiboot_tag_framebuffer));

    Must(load_driver(create_dev_manifest((aniva_driver_t*)&g_base_kterm_driver, 0)));

  }
}

static void root_packet_dispatch() {

  for (;;) {

    pause_scheduler();
    threaded_socket_t* socket = socket_peek_messaged();

    if (socket != nullptr) {
      ErrorOrPtr result = socket_handle_packet(socket);
      if (result.m_status == ANIVA_SUCCESS) {
        // remove the entry once we processed its packet
        // successfuly
        socket_grab_messaged();
      }
    }

    resume_scheduler();
    // after one swoop we don't need to check again lmao
    scheduler_yield();
  }

  kernel_panic("TRIED TO EXIT root_packet_dispatch!");
}
