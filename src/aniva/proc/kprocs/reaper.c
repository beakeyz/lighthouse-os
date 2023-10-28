#include "reaper.h"
#include "dev/debug/serial.h"
#include "dev/pci/io.h"
#include "libk/flow/error.h"
#include "libk/data/queue.h"
#include "logging/log.h"
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
static void USED reaper_main() 
{
  proc_t* proc;

  /* Check if we actually have a queue, otherwise try to create it again */
  if (!__reaper_queue)
    __reaper_queue = create_limitless_queue();

  if (!__reaper_lock)
    __reaper_lock = create_mutex(0);
  
  ASSERT_MSG(__reaper_queue, "Could not start reaper: unable to create queue");
  ASSERT_MSG(__reaper_lock, "Could not start reaper: unable to create mutex");

  /* Simply pass execution through */
  while (true) {

    /* FIXME: Locking issue; trying to lock here seems to cause a deadlock somewhere? */
    mutex_lock(__reaper_lock);

    proc = queue_dequeue(__reaper_queue);

    if (proc)
      destroy_proc(proc);

    mutex_unlock(__reaper_lock);

    scheduler_yield();
  }

  kernel_panic("Reaper thread isn't supposed to exit its loop!");
}

static void USED reaper_exit() {
  kernel_panic("Reaper thread isn't supposed to exit!");
}

/*!
 * @brief: Asyncronically destroy a process and remove it from the scheduler
 *
 * 
 */
ErrorOrPtr reaper_register_process(proc_t* proc) 
{
  if (!proc)
    return Error();

  if (!__reaper_thread)
    return Error();

  /* TODO: If the reaper thread is idle, wake it up */
  ASSERT_MSG(!(__reaper_thread->m_parent_proc->m_flags & PROC_IDLE), "Kernelprocess seems to be idle!");
  
  /* Get the reaper lock so we know we can safely queue up the process */
  mutex_lock(__reaper_lock);

  /* Mark as finished, since we know we won't be seeing it again after we return from this call */
  proc->m_flags |= PROC_FINISHED;

  /* Ring the doorbell, so waiters know this process has terminated (Do this before we kill process memory) */
  doorbell_ring(proc->m_terminate_bell);

  /* Remove from the scheduler (Pauses it) */
  (void)sched_remove_proc(proc);

  /* Queue the process to the reaper */
  queue_enqueue(__reaper_queue, proc);

  /* Unlock the mutex. After this we musn't access @proc anymore */
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
  __reaper_thread = create_thread_for_proc(proc, reaper_main, NULL, "Reaper");

  /* Create the mutex that ensures safety inside the reaper */
  __reaper_lock = create_mutex(NULL);

  /* Queue with wich processes will be passed through */
  __reaper_queue = create_limitless_queue();

  proc_add_thread(proc, __reaper_thread);

  return Success(0);
}
