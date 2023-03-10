#include "root_process.h"
#include "dev/core.h"
#include "dev/debug/serial.h"
#include "dev/framebuffer/framebuffer.h"
#include "interupts/interupts.h"
#include "kmain.h"
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
#include <dev/manifest.h>

static void root_main(uintptr_t multiboot_address);
static void root_packet_dispatch();

void create_and_register_root_process(uintptr_t multiboot_address) {

  proc_t* root_proc = create_kernel_proc(root_main, multiboot_address);
  set_kernel_proc(root_proc);

  // base drivers get hooked on the main processor and its kernel process
  register_aniva_base_drivers();

  proc_add_thread(root_proc, create_thread_for_proc(root_proc, root_packet_dispatch, NULL, "root packet dispatch"));

  sched_add_proc(root_proc);
}

static void root_main(uintptr_t multiboot_address) {

  if (multiboot_address) {
    load_driver(create_dev_manifest((aniva_driver_t*)&g_base_fb_driver, 0));
    driver_send_packet("graphics.fb", FB_DRV_SET_MB_TAG, get_mb2_tag((void*)multiboot_address, MULTIBOOT_TAG_TYPE_FRAMEBUFFER), sizeof(struct multiboot_tag_framebuffer));
    uintptr_t ptr = 0xFFFFFFFFFF600000;
    destroy_packet_response(await(driver_send_packet("graphics.fb", FB_DRV_MAP_FB, &ptr, sizeof(uintptr_t))));
  }
}

static void root_packet_dispatch() {

  for (;;) {
    list_t sockets = get_registered_sockets();

    FOREACH(i, &sockets) {
      threaded_socket_t* socket = i->data;

      socket_handle_packets(socket);
    }
    // after one swoop we don't need to check again lmao
    scheduler_yield();
  }

  kernel_panic("TRIED TO EXIT root_packet_dispatch!");
}
