#include "spinlock.h"
#include "kmain.h"
#include "mem/kmalloc.h"
#include "sync/atomic_ptr.h"

// FIXME: make this wrapper actually useful
spinlock_t* init_spinlock() {
  spinlock_t* lock = kmalloc(sizeof(spinlock_t));

  //lock->m_processor = get_current_processor();
  //lock->m_thread = get_current_processor()->m_root_thread;
  //lock->m_proc = get_current_processor()->m_root_thread->m_parent_proc;

  lock->m_lock = __init_spinlock();
  return lock;
}

void lock_spinlock(spinlock_t* lock) {
  Processor_t* current_processor = get_current_processor();
  uintptr_t j = atomic_ptr_load(current_processor->m_locked_level);
  atomic_ptr_write(current_processor->m_locked_level, j+1);
  // TODO: lock
  aquire_spinlock(&lock->m_lock);
}
void unlock_spinlock(spinlock_t* lock) {
  Processor_t* current_processor = get_current_processor();
  uintptr_t j = atomic_ptr_load(current_processor->m_locked_level);
  atomic_ptr_write(current_processor->m_locked_level, j-1);
  release_spinlock(&lock->m_lock);
}
bool is_spinlock_locked(spinlock_t* lock) {
  return lock->m_lock.m_cpu_num >= 0;
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
  while (__sync_lock_test_and_set(lock->m_latch, 0x01));
  lock->m_cpu_num = (int)get_current_processor()->m_cpu_num;
  lock->m_func = __func__;
}

void release_spinlock(__spinlock_t* lock)
{
  lock->m_func = nullptr;
  lock->m_cpu_num = -1;
  __sync_lock_release(lock->m_latch);
}
