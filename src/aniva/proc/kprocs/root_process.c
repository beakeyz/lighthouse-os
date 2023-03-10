#include "root_process.h"
#include "dev/core.h"
#include "dev/debug/serial.h"
#include "interupts/interupts.h"
#include "kmain.h"
#include "libk/error.h"
#include "libk/io.h"
#include "libk/linkedlist.h"
#include "proc/core.h"
#include "proc/ipc/tspckt.h"
#include "proc/proc.h"
#include "proc/socket.h"
#include "proc/thread.h"
#include "sched/scheduler.h"
#include "sync/mutex.h"
#include "sync/spinlock.h"

static void root_main(uintptr_t multiboot_address);
static void root_packet_dispatch();

void create_and_register_root_process(uintptr_t multiboot_address) {

  proc_t* root_proc = create_kernel_proc(root_main, NULL);

  proc_add_thread(root_proc, create_thread_for_proc(root_proc, root_packet_dispatch, multiboot_address, "root packet dispatch"));

  sched_add_proc(root_proc);
}

static void root_main(uintptr_t multiboot_address) {

  register_aniva_base_drivers();

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
