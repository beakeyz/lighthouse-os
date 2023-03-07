#include "mutex.h"
#include "dev/debug/serial.h"
#include "libk/error.h"
#include "libk/linkedlist.h"
#include "libk/queue.h"
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

mutex_t* create_mutex(uint8_t flags) {
  mutex_t* ret = kmalloc(sizeof(mutex_t));

  ret->m_waiters = create_limitless_queue();
  ret->m_lock = create_spinlock();
  ret->m_lock_holder = nullptr;
  ret->m_mutex_flags = flags;

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

  if (mutex->m_mutex_flags & MUTEX_FLAG_IS_HELD) {

    if (current_thread != mutex->m_lock_holder) {
      // block current thread
      queue_enqueue(mutex->m_waiters, current_thread);

      // NOTE: when we block this thread, it returns executing here after it gets unblocked by mutex_unlock,
      // since we just yield to the scheduler when we're blocked
      println("blocking to take lock");

      if (spinlock_is_locked(mutex->m_lock))
        spinlock_unlock(mutex->m_lock);

      thread_block(current_thread);

      ASSERT_MSG(mutex->m_lock_depth == 0, "Mutex was not unlocked after thread got unblocked!");
    }
  }

  println("taking lock");
  // take lock
  ASSERT_MSG(mutex->m_lock_depth == 0, "Tried to take a mutex while it has a locked depth greater than 0!");

  mutex->m_mutex_flags |= MUTEX_FLAG_IS_HELD;
  mutex->m_lock_holder = current_thread;

  mutex->m_lock_depth++;

  if (spinlock_is_locked(mutex->m_lock))
    spinlock_unlock(mutex->m_lock);
}

void mutex_unlock(mutex_t* mutex) {

  ASSERT_MSG(get_current_processor()->m_irq_depth == 0, "Can't lock a mutex from within an IRQ!");
  ASSERT_MSG(mutex->m_lock_holder != nullptr, "mutex has no holder while trying to unlock!");
  ASSERT_MSG(mutex->m_lock_depth > 0, "Tried to unlock a mutex while it was already unlocked!");
  ASSERT_MSG((mutex->m_mutex_flags & MUTEX_FLAG_IS_HELD), "IS_HELD flag not set while trying to unlock mutex");

  spinlock_lock(mutex->m_lock);

  thread_t* current_thread = get_current_scheduling_thread();

  mutex->m_lock_depth--;

  if (mutex->m_lock_depth == 0) {

    mutex->m_mutex_flags &= ~MUTEX_FLAG_IS_HELD;
    __mutex_handle_unblock(mutex);
  }

  if (spinlock_is_locked(mutex->m_lock))
    spinlock_unlock(mutex->m_lock);
}

// FIXME: inline?
bool mutex_is_locked(mutex_t* mutex) {
  return (mutex->m_lock_depth > 0 && (mutex->m_mutex_flags & MUTEX_FLAG_IS_HELD));
}

// FIXME: inline?
bool mutex_is_locked_by_current_thread(mutex_t* mutex) {
  return (mutex_is_locked(mutex) && mutex->m_lock_holder == get_current_scheduling_thread());
}

static void __mutex_handle_unblock(mutex_t* mutex) {
  // TODO:

  thread_t* next_holder = queue_dequeue(mutex->m_waiters);

  ASSERT_MSG(next_holder != get_current_scheduling_thread(), "Next thread to hold mutex is also the current thread!");

  if (next_holder) {
    thread_unblock(next_holder);
    mutex->m_lock_holder = next_holder;
  }
}
