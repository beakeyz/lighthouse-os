#include "reaper.h"
#include "libk/flow/error.h"
#include "libk/data/queue.h"
#include "libk/stddef.h"
#include "proc/proc.h"
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
  thread_t* c_thread;

  /* Check if we actually have a queue, otherwise try to create it again */
  if (!__reaper_queue)
    __reaper_queue = create_limitless_queue();

  if (!__reaper_lock)
    __reaper_lock = create_mutex(0);

  /* Get the current thread, just to be safe */
  c_thread = get_current_scheduling_thread();
  (void)c_thread;
  
  ASSERT_MSG(__reaper_queue, "Could not start reaper: unable to create queue");
  ASSERT_MSG(__reaper_lock, "Could not start reaper: unable to create mutex");

  while (true) {

    mutex_lock(__reaper_lock);

    while ((proc = queue_dequeue(__reaper_queue)) != nullptr) {

      printf("[REAPER]: Trying to destroy process %s\n", proc->m_name);

      /* Buh bye */
      destroy_proc(proc);
    }

    mutex_unlock(__reaper_lock);

    /* zzzzzz */
    //thread_sleep(c_thread);
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
  
  /* Get the reaper lock so we know we can safely queue up the process */
  mutex_lock(__reaper_lock);

  /* Queue the process to the reaper */
  queue_enqueue(__reaper_queue, proc);

  /* Wake the thread if it's sleeping */
  //thread_wakeup(__reaper_thread);

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

int reaper_wake()
{
  // TODO:
  return 0;
}
