#include "reaper.h"
#include "dev/debug/serial.h"
#include "libk/error.h"
#include "libk/queue.h"
#include "proc/core.h"
#include "proc/ipc/packet_payload.h"
#include "proc/ipc/packet_response.h"
#include "proc/proc.h"
#include "proc/socket.h"
#include "proc/thread.h"
#include "sched/scheduler.h"

static thread_t* __reaper_thread;
static uint32_t __reaper_port;

/*
 * TODO: should we have process destruction happen here, so execution isn't blocked,
 * or in reaper_on_packet, where we block untill the process is destroyed AND we're 
 * most likely in an interrupt / critical section
 */
static void USED reaper_main() {

  /* Simply pass execution through */
  for (;;) {
    scheduler_yield();
  }
  kernel_panic("Reaper thread isn't supposed to exit its loop!");
}

static void USED reaper_exit() {
  kernel_panic("Reaper thread isn't supposed to exit!");
}

static uintptr_t USED reaper_on_packet(packet_payload_t payload, packet_response_t** respose) {

  proc_t* process = payload.m_data;

  if (proc_can_schedule(process))
    return -1;

  println("Reaper: destroying process!");

  /* Now we can perform the dirty work */
  destroy_proc(process);

  return 0;
}

ErrorOrPtr reaper_register_process(proc_t* proc) {

  if (!proc || proc_can_schedule(proc))
    return Error();

  if (!__reaper_thread)
    return Error();

  /* TODO: If the reaper thread is idle, wake it up */
  ASSERT_MSG(!(__reaper_thread->m_parent_proc->m_flags & PROC_IDLE), "Kernelprocess seems to be idle!");

  TRY(send_result, send_packet_to_socket_no_response(__reaper_port, 0, proc, sizeof(proc_t*)));

  return Success(0);
}

ErrorOrPtr init_reaper(proc_t* proc) {
  
  if (!proc || !(proc->m_flags & PROC_KERNEL))
    return Error();

  /* Make sure we know this is the process that contains the reaper */
  proc->m_flags |= PROC_REAPER;

  __reaper_port = 0;

  __reaper_thread = create_thread_as_socket(proc, reaper_main, NULL, reaper_exit, reaper_on_packet, "Reaper", &__reaper_port);

  proc_add_thread(proc, __reaper_thread);

  return Success(0);
}
