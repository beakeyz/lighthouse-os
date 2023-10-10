#include "spinlock.h"
#include "dev/debug/serial.h"
#include "sched/scheduler.h"
#include "sync/atomic_ptr.h"
#include "system/processor/processor.h"
#include <mem/heap.h>
#include <libk/string.h>

/* __spinlock_t */
static __spinlock_t __init_spinlock();
static void aquire_spinlock(__spinlock_t* lock);
static void release_spinlock(__spinlock_t* lock);

// FIXME: make this wrapper actually useful
spinlock_t* create_spinlock() {
  spinlock_t* lock = kmalloc(sizeof(spinlock_t));

  lock->m_is_locked = create_atomic_ptr_with_value(false);
  //lock->m_thread = get_current_processor()->m_root_thread;
  //lock->m_proc = get_current_processor()->m_root_thread->m_parent_proc;

  lock->m_lock = __init_spinlock();
  return lock;
}

void destroy_spinlock(spinlock_t* lock) {
  destroy_atomic_ptr(lock->m_is_locked);

  memset(lock, 0, sizeof(spinlock_t));
  kfree(lock);
}

/*
 * FIXME: should we limit spinlock usage in IRQs? 
 * could they pose a risk for potential deadlocks?
 */
void spinlock_lock(spinlock_t* lock) {
  aquire_spinlock(&lock->m_lock);

  lock->m_processor = get_current_processor();

  uintptr_t j = atomic_ptr_load(lock->m_processor->m_locked_level);
  atomic_ptr_write(lock->m_processor->m_locked_level, j+1);
  atomic_ptr_write(lock->m_is_locked, true);
}

void spinlock_unlock(spinlock_t* lock) {
  uintptr_t j = atomic_ptr_load(lock->m_processor->m_locked_level);
  ASSERT_MSG(j > 0, "unlocking spinlock while having a m_locked_level of 0!");

  atomic_ptr_write(lock->m_processor->m_locked_level, j-1);
  atomic_ptr_write(lock->m_is_locked, false);

  lock->m_processor = nullptr;

  release_spinlock(&lock->m_lock);
}

bool spinlock_is_locked(spinlock_t* lock) {
  return atomic_ptr_load(lock->m_is_locked) == true;
}

/* __spinlock_t */

__spinlock_t __init_spinlock() {
  __spinlock_t ret = {
    .m_latch[0] = 0,
    .m_cpu_num = -1,
    .m_func = nullptr
  };
  return ret;
}

void aquire_spinlock(__spinlock_t* lock)
{
  processor_t* current;

  if (!lock)
    return;

  current = get_current_processor();

  while (__sync_lock_test_and_set(lock->m_latch, 0x01)) {

    ASSERT_MSG(current->m_irq_depth == 0, "Can't block on a spinlock from within an IRQ!");
    /* FIXME: should we? */
    scheduler_yield();
  }
  lock->m_cpu_num = (int)current->m_cpu_num;
  lock->m_func = __func__;
}

void release_spinlock(__spinlock_t* lock)
{
  lock->m_func = nullptr;
  lock->m_cpu_num = -1;
  __sync_lock_release(lock->m_latch);
}
