#include "mutex.h"
#include "libk/flow/error.h"
#include "libk/data/queue.h"
#include "mem/heap.h"
#include "proc/thread.h"
#include "sched/scheduler.h"
#include "sync/spinlock.h"
#include "system/processor/processor.h"

/*
 * Wrapper that unblocks the threads that tried to 
 * take the mutex while it was locked
 */
static void __mutex_handle_unblock(mutex_t* mutex);

mutex_t* create_mutex(uint8_t flags) 
{
  mutex_t* ret;

  ret = kmalloc(sizeof(mutex_t));

  ret->m_waiters = create_limitless_queue();
  ret->m_lock = create_spinlock();
  ret->m_lock_holder = nullptr;
  ret->m_mutex_flags = flags;
  ret->m_lock_depth = 0;

  if (flags & MUTEX_FLAG_IS_HELD) {
    flags &= ~MUTEX_FLAG_IS_HELD;

    mutex_lock(ret);
  }

  return ret;
}

void init_mutex(mutex_t* lock, uint8_t flags)
{
  lock->m_waiters = create_limitless_queue();
  lock->m_lock = create_spinlock();
  lock->m_lock_holder = nullptr;
  lock->m_mutex_flags = flags;
  lock->m_lock_depth = 0;

  if (flags & MUTEX_FLAG_IS_HELD) {
    flags &= ~MUTEX_FLAG_IS_HELD;

    mutex_lock(lock);
  }
}

void clear_mutex(mutex_t* mutex)
{
  thread_t* waiter;

  if (!mutex || !mutex->m_waiters)
    return;

  /*
   * FIXME: This can get nasty if there are other threads
   * waiting to do stuff with this mutex, because the
   * object that this mutex protects is then unprotected for a time
   */
  waiter = queue_dequeue(mutex->m_waiters);

  while (waiter) {

    kernel_panic("TODO: figure out what to do with waiter threads on a mutex when the mutex is destroyed");
    //waiter->m_current_state = DYING;
    //thread_unblock(waiter);
    
    waiter = queue_dequeue(mutex->m_waiters);
  }

  destroy_queue(mutex->m_waiters, false);
}

void destroy_mutex(mutex_t* mutex) 
{
  if (!mutex)
    return;

  thread_unregister_mutex(mutex->m_lock_holder, mutex);

  clear_mutex(mutex);

  destroy_spinlock(mutex->m_lock);
  kfree(mutex);
}

void mutex_lock(mutex_t* mutex) 
{
  thread_t* current_thread;
  ASSERT_MSG(mutex, "Tried to lock a mutex that has not been initialized");

  current_thread = get_current_scheduling_thread();

  if (!current_thread)
    return;

  thread_register_mutex(current_thread, mutex);

retry_lock:
  spinlock_lock(mutex->m_lock);

  // NOTE: it's fine to be preempted here, since this value won't change
  // when we get back
  //current_thread = get_current_scheduling_thread();

  if (current_thread == mutex->m_lock_holder) {
    mutex->m_lock_depth++;
    goto exit_mutex_locking;
  }

  if (mutex->m_lock_depth > 0) {

    ASSERT_MSG(mutex->m_lock_holder != nullptr, "Mutex is locked, but had no holder!");

    /* 
     * This also works outside the scheduler context, since we won't block threads ever 
     * if they are null 
     */
    if (current_thread != mutex->m_lock_holder) {

      /* We may lock a mutex from within a irq, but we cant block on it */
      ASSERT_MSG(get_current_processor()->m_irq_depth == 0, "Can't block on a mutex from within an IRQ!");

      // block current thread
      queue_enqueue(mutex->m_waiters, current_thread);

      // NOTE: when we block this thread, it returns executing here after it gets unblocked by mutex_unlock,
      // since we just yield to the scheduler when we're blocked

      spinlock_unlock(mutex->m_lock);

      thread_block(current_thread);

      goto retry_lock;

      /*
      spinlock_lock(mutex->m_lock);

      ASSERT_MSG(mutex->m_lock_depth == 0, "Mutex was not unlocked after thread got unblocked!");
      */
    }
  }

  // take lock
  // FIXME: remove this assert and propperly support multi-depth mutex locking
  //ASSERT_MSG(mutex->m_lock_depth == 0, "Tried to take a mutex while it has a locked depth greater than 0!");

  mutex->m_mutex_flags |= MUTEX_FLAG_IS_HELD;
  mutex->m_lock_holder = current_thread;

  mutex->m_lock_depth++;

exit_mutex_locking:
  spinlock_unlock(mutex->m_lock);
}

void mutex_unlock(mutex_t* mutex) 
{
  thread_t* c_thread;

  c_thread = get_current_scheduling_thread();

  if (!c_thread)
    return;

  ASSERT_MSG(mutex, "Tried to unlock a mutex that has not been initialized");
  //ASSERT_MSG(get_current_processor()->m_irq_depth == 0, "Can't lock a mutex from within an IRQ!");
  // ASSERT_MSG(mutex->m_lock_holder != nullptr, "mutex has no holder while trying to unlock!");
  ASSERT_MSG(mutex->m_lock_depth, "Tried to unlock a mutex while it was already unlocked!");
  ASSERT_MSG((mutex->m_mutex_flags & MUTEX_FLAG_IS_HELD), "IS_HELD flag not set while trying to unlock mutex");

  /* This assert triggers unwillingly when we unlock locked mutexes on thread exit */
  //ASSERT_MSG(mutex->m_lock_holder == c_thread, "Tried to unlock a mutex that wasn't locked by this thread");

  spinlock_lock(mutex->m_lock);

  mutex->m_lock_depth--;

  if (!mutex->m_lock_depth) {

    mutex->m_mutex_flags &= ~MUTEX_FLAG_IS_HELD;

    thread_unregister_mutex(mutex->m_lock_holder, mutex);

    /* Unlock the spinlock before we try unblocking */
    spinlock_unlock(mutex->m_lock);

    /* Unblock */
    __mutex_handle_unblock(mutex);
    return;
  }

  spinlock_unlock(mutex->m_lock);
}

// FIXME: inline?
bool mutex_is_locked(mutex_t* mutex) {
  // No mutex means no lock =/
  if (!mutex)
    return false;
  return (mutex->m_lock_depth > 0 && (mutex->m_mutex_flags & MUTEX_FLAG_IS_HELD));
}

// FIXME: inline?
bool mutex_is_locked_by_current_thread(mutex_t* mutex) {
  return (mutex_is_locked(mutex) && mutex->m_lock_holder == get_current_scheduling_thread());
}

/*!
 * @brief: Handle the cycling of waiters
 *
 * NOTE: the caller must have the mutexes spinlock held
 */
static void __mutex_handle_unblock(mutex_t* mutex) 
{
  thread_t* current_thread;
  thread_t* next_holder;

  if (!mutex)
    return;

  /* Preemptive reset of the holder field */
  mutex->m_lock_holder = nullptr;

  current_thread = get_current_scheduling_thread();
  next_holder = queue_dequeue(mutex->m_waiters);

  if (!current_thread || !next_holder)
    return;

  pause_scheduler();

  ASSERT_MSG(next_holder != current_thread, "Next thread to hold mutex is also the current thread!");

  mutex->m_lock_holder = next_holder;

  thread_unblock(next_holder);

  resume_scheduler();

  scheduler_yield();
}
