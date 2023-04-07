#include "root_process.h"
#include "dev/core.h"
#include "dev/debug/serial.h"
#include "dev/debug/test.h"
#include "dev/disk/ahci/ahci_device.h"
#include "dev/framebuffer/framebuffer.h"
#include "dev/keyboard/ps2_keyboard.h"
#include "dev/kterm/kterm.h"
#include "fs/vfs.h"
#include "interupts/interupts.h"
#include "libk/async_ptr.h"
#include "libk/error.h"
#include "libk/io.h"
#include "libk/linkedlist.h"
#include "libk/multiboot.h"
#include "libk/queue.h"
#include "libk/stddef.h"
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

static void root_main() {

  for (;;) {
  }
}

void create_and_register_root_process() {

  proc_t* root_proc = create_kernel_proc(root_main, NULL);
  set_kernel_proc(root_proc);

  // base drivers get hooked on the main processor and its kernel process

  // proc_add_thread(root_proc, create_thread_for_proc(root_proc, root_packet_dispatch, NULL, "root packet dispatch"));

  sched_add_proc(root_proc);

}
