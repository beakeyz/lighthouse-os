#include "mutex.h"
#include "libk/data/linkedlist.h"
#include "libk/flow/error.h"
#include "libk/data/queue.h"
#include "libk/stddef.h"
#include "mem/heap.h"
#include "proc/thread.h"
#include "sched/scheduler.h"
#include "sync/spinlock.h"
#include "system/processor/processor.h"

/*
 * Wrapper that unblocks the threads that tried to 
 * take the mutex while it was locked
 */
static int __mutex_handle_unblock(mutex_t* mutex);

mutex_t* create_mutex(uint8_t flags) 
{
  mutex_t* ret;

  ret = kmalloc(sizeof(mutex_t));

  if (!ret)
    return nullptr;

  memset(ret, 0, sizeof(*ret));

  ret->m_waiters = create_limitless_queue();
  ret->m_lock = create_spinlock();
  ret->m_lock_holder = nullptr;

  return ret;
}

void init_mutex(mutex_t* lock, uint8_t flags)
{
  memset(lock, 0, sizeof(*lock));

  lock->m_waiters = create_limitless_queue();
  lock->m_lock = create_spinlock();
  lock->m_lock_holder = nullptr;
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

  ASSERT_MSG(mutex->m_lock_holder == nullptr && mutex->m_lock_depth == 0, "Tried to destroy a locked mutex");

  /* Clear any pending waiters */
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

  do {
    spinlock_lock(mutex->m_lock);

    if (!mutex->m_lock_holder)
      break;

    /* 
     * This also works outside the scheduler context, since we won't block threads ever 
     * if they are null 
     */
    if (current_thread == mutex->m_lock_holder)
      break;

    /* We may lock a mutex from within a irq, but we cant block on it */
    ASSERT_MSG(get_current_processor()->m_irq_depth == 0, "Can't block on a mutex from within an IRQ!");

    // block current thread
    queue_enqueue(mutex->m_waiters, current_thread);

    // NOTE: when we block this thread, it returns executing here after it gets unblocked by mutex_unlock,
    // since we just yield to the scheduler when we're blocked

    spinlock_unlock(mutex->m_lock);

    thread_block(current_thread);

  } while(true);


  if (!mutex->m_lock_depth)
    list_append(current_thread->m_locked_mutexes, mutex);

  mutex->m_lock_holder = current_thread;
  mutex->m_lock_depth++;
  spinlock_unlock(mutex->m_lock);
}

void mutex_unlock(mutex_t* mutex) 
{
  thread_t* c_thread;

  c_thread = get_current_scheduling_thread();

  if (!c_thread)
    return;

  ASSERT_MSG(mutex && mutex->m_lock, "Tried to unlock a mutex that has not been initialized");

  spinlock_lock(mutex->m_lock);

  ASSERT_MSG(mutex->m_lock_depth--, "Tried to unlock a mutex while it was already unlocked!");

  if (!mutex->m_lock_depth) {

    //thread_unregister_mutex(mutex->m_lock_holder, mutex);
    if (mutex->m_lock_holder)
      (void)list_remove_ex(mutex->m_lock_holder->m_locked_mutexes, mutex);

    /* Unblock */
    if (__mutex_handle_unblock(mutex) == 0)
      return;
  }

  spinlock_unlock(mutex->m_lock);
}

/*!
 * @brief: Removes a thread from a mutexes waiters queue
 */
void mutex_thread_release(mutex_t* mutex, thread_t* t)
{
  spinlock_lock(mutex->m_lock);

  if (mutex->m_lock_holder == t) {
    mutex->m_lock_depth = 0;

    /* Unblock */
    if (__mutex_handle_unblock(mutex) == 0)
      return;
  }

  /* Try to remove t from the waiters */
  (void)queue_remove(mutex->m_waiters, t);

  spinlock_unlock(mutex->m_lock);
}

// FIXME: inline?
bool mutex_is_locked(mutex_t* mutex) 
{
  return (mutex && mutex->m_lock_holder);
}

// FIXME: inline?
bool mutex_is_locked_by_current_thread(mutex_t* mutex) 
{
  return (mutex && mutex->m_lock_holder == get_current_scheduling_thread());
}

/*!
 * @brief: Handle the cycling of waiters
 *
 * NOTE: the caller must have the mutexes spinlock held
 */
static int __mutex_handle_unblock(mutex_t* mutex)
{
  thread_t* next_holder;

  if (!mutex)
    return -1;

  /* Preemptive reset of the holder field */
  mutex->m_lock_holder = nullptr;

  next_holder = queue_dequeue(mutex->m_waiters);

  if (!next_holder)
    return -1;

  // NOTE: Never seems to happen
  //ASSERT_MSG(next_holder != current_thread, "Next thread to hold mutex is also the current thread!");

  /* Set the next holder */
  mutex->m_lock_holder = next_holder;

  /* Release the mutext */
  spinlock_unlock(mutex->m_lock);

  /* Mark the thread unblocked */
  thread_unblock(next_holder);

  /* Try to yield to the scheduler */
  scheduler_yield();

  return 0;
}
