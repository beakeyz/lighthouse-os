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
#include "sync/mutex.h"

static thread_t* __reaper_thread;
static uint32_t __reaper_port;
static mutex_t* __reaper_lock;
static queue_t* __reaper_queue;

/*
 * TODO: should we have process destruction happen here, so execution isn't blocked,
 * or in reaper_on_packet, where we block untill the process is destroyed AND we're 
 * most likely in an interrupt / critical section
 */
static void USED reaper_main() {

  /* Check if we actually have a queue, otherwise try to create it again */
  if (!__reaper_queue)
    __reaper_queue = create_limitless_queue();

  if (!__reaper_lock)
    __reaper_lock = create_mutex(0);
  
  ASSERT_MSG(__reaper_queue, "Could not start reaper: unable to create queue");
  ASSERT_MSG(__reaper_lock, "Could not start reaper: unable to create mutex");

  /* Simply pass execution through */
  for (;;) {
    proc_t* proc = queue_dequeue(__reaper_queue);

    if (proc) {
      /* FIXME: Locking issue; trying to lock here seems to cause a deadlock somewhere? */
      mutex_lock(__reaper_lock);

      destroy_proc(proc);

      mutex_unlock(__reaper_lock);
    }

    scheduler_yield();
  }
  kernel_panic("Reaper thread isn't supposed to exit its loop!");
}

static void USED reaper_exit() {
  kernel_panic("Reaper thread isn't supposed to exit!");
}

/*
 * Let's still allow direct messaging to the reaper socket for now,
 * but let's 
 * TODO: remove socket status if we don't find a use for this soon
 */
static uintptr_t USED reaper_on_packet(packet_payload_t payload, packet_response_t** respose) {

  proc_t* process = payload.m_data;

  if (!process || payload.m_data_size != sizeof(proc_t*))
    return -1;

  if (proc_can_schedule(process))
    return -1;


  /*
   * NOTE: these data structures are non-atomic, so 
   *       opperations like these are generaly unsafe.
   *       here, I'll allow it for now, since the way packets
   *       work right now ensure no scheduling on this CPU,
   *       since packets work on interrupts
   */
  queue_enqueue(__reaper_queue, process);

  return 0;
}

ErrorOrPtr reaper_register_process(proc_t* proc) {

  if (!proc || proc_can_schedule(proc))
    return Error();

  if (!__reaper_thread)
    return Error();

  /* TODO: If the reaper thread is idle, wake it up */
  ASSERT_MSG(!(__reaper_thread->m_parent_proc->m_flags & PROC_IDLE), "Kernelprocess seems to be idle!");

  //TRY(send_result, send_packet_to_socket_no_response(__reaper_port, 0, proc, sizeof(proc_t*)));
  
  mutex_lock(__reaper_lock);

  queue_enqueue(__reaper_queue, proc);

  mutex_unlock(__reaper_lock);

  return Success(0);
}

ErrorOrPtr init_reaper(proc_t* proc) {
  
  if (!proc || !(proc->m_flags & PROC_KERNEL))
    return Error();

  /* Make sure we know this is the process that contains the reaper */
  proc->m_flags |= PROC_REAPER;

  __reaper_port = 0;

  /*
   * Create the thread that our reaper will live on
   * repeat TODO: find usage for this thread being a socket, otherwise remove
   * perhaps we can even just move packets off of the interrupts,
   * so we can just lock mutexes and crap
   */
  __reaper_thread = create_thread_as_socket(proc, reaper_main, NULL, reaper_exit, reaper_on_packet, "Reaper", &__reaper_port);

  /* Create the mutex that ensures safety inside the reaper */
  __reaper_lock = create_mutex(NULL);

  /* Queue with wich processes will be passed through */
  __reaper_queue = create_limitless_queue();

  proc_add_thread(proc, __reaper_thread);

  return Success(0);
}
