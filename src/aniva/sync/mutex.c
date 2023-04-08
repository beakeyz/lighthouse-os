#include "mutex.h"
#include "dev/debug/serial.h"
#include "interupts/interupts.h"
#include "libk/error.h"
#include "libk/linkedlist.h"
#include "libk/queue.h"
#include "libk/string.h"
#include "mem/heap.h"
#include "mem/kmem_manager.h"
#include "proc/thread.h"
#include "sched/scheduler.h"
#include "sync/spinlock.h"
#include "system/processor/processor.h"

/*
 * Wrapper that unblocks the threads that tried to 
 * take the mutex while it was locked
 */
static void __mutex_handle_unblock(mutex_t* mutex);

mutex_t* create_mutex(uint8_t flags) {
  mutex_t* ret = kmalloc(sizeof(mutex_t));

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

void destroy_mutex(mutex_t* mutex) {

  destroy_queue(mutex->m_waiters, false);
  destroy_spinlock(mutex->m_lock);
  kfree(mutex);

}

void mutex_lock(mutex_t* mutex) {
  ASSERT_MSG(get_current_processor()->m_irq_depth == 0, "Can't lock a mutex from within an IRQ!");

  spinlock_lock(mutex->m_lock);

  // NOTE: it's fine to be preempted here, since this value won't change
  // when we get back
  thread_t* current_thread = get_current_scheduling_thread();

  if (mutex->m_lock_depth > 0) {

    ASSERT_MSG(mutex->m_lock_holder != nullptr, "Mutex is locked, but had no holder!");

    /* 
     * This also works outside the scheduler context, since we won't block threads ever 
     * if they are null 
     */
    if (current_thread != mutex->m_lock_holder) {
      // block current thread
      queue_enqueue(mutex->m_waiters, current_thread);

      // NOTE: when we block this thread, it returns executing here after it gets unblocked by mutex_unlock,
      // since we just yield to the scheduler when we're blocked

      spinlock_unlock(mutex->m_lock);

      thread_block(current_thread);

      spinlock_lock(mutex->m_lock);

      ASSERT_MSG(mutex->m_lock_depth == 0, "Mutex was not unlocked after thread got unblocked!");
    }
  }

  // take lock
  // FIXME: remove this assert and propperly support multi-depth mutex locking
  ASSERT_MSG(mutex->m_lock_depth == 0, "Tried to take a mutex while it has a locked depth greater than 0!");

  mutex->m_mutex_flags |= MUTEX_FLAG_IS_HELD;
  mutex->m_lock_holder = current_thread;

  mutex->m_lock_depth++;

  spinlock_unlock(mutex->m_lock);
}

void mutex_unlock(mutex_t* mutex) {

  ASSERT_MSG(get_current_processor()->m_irq_depth == 0, "Can't lock a mutex from within an IRQ!");
  // ASSERT_MSG(mutex->m_lock_holder != nullptr, "mutex has no holder while trying to unlock!");
  ASSERT_MSG(mutex->m_lock_depth > 0, "Tried to unlock a mutex while it was already unlocked!");
  ASSERT_MSG((mutex->m_mutex_flags & MUTEX_FLAG_IS_HELD), "IS_HELD flag not set while trying to unlock mutex");

  spinlock_lock(mutex->m_lock);

  thread_t* current_thread = get_current_scheduling_thread();

  mutex->m_lock_depth--;

  if (mutex->m_lock_depth == 0) {

    mutex->m_mutex_flags &= ~MUTEX_FLAG_IS_HELD;
    __mutex_handle_unblock(mutex);
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

static void __mutex_handle_unblock(mutex_t* mutex) {
  // TODO:

  thread_t* current_thread = get_current_scheduling_thread();
  thread_t* next_holder = queue_dequeue(mutex->m_waiters);

  if (current_thread == nullptr) {
    goto reset_holder;
  }

  ASSERT_MSG(next_holder != current_thread, "Next thread to hold mutex is also the current thread!");

  if (next_holder) {
    thread_unblock(next_holder);
    mutex->m_lock_holder = next_holder;
    return;
  }

reset_holder:
  mutex->m_lock_holder = nullptr;
}
