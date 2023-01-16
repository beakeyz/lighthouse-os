#include "spinlock.h"
#include "kmain.h"
#include "mem/kmalloc.h"
#include "sync/atomic_ptr.h"

spinlock_t* init_spinlock() {
  spinlock_t* lock = kmalloc(sizeof(spinlock_t));

  //lock->m_processor = g_GlobalSystemInfo.m_current_core;
  //lock->m_thread = g_GlobalSystemInfo.m_current_core->m_root_thread;
  //lock->m_proc = g_GlobalSystemInfo.m_current_core->m_root_thread->m_parent_proc;

  lock->m_lock = __init_spinlock();

  return lock;
}

void lock_spinlock(spinlock_t* lock) {
  Processor_t* current_processor = g_GlobalSystemInfo.m_current_core;
  uintptr_t j = atomic_ptr_load(current_processor->m_locked_level);
  atomic_ptr_write(current_processor->m_locked_level, j+1);
  // TODO: lock
}
void unlock_spinlock(spinlock_t* lock) {

}
bool is_spinlock_locked(spinlock_t* lock) {

  return false;
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
  lock->m_cpu_num = g_GlobalSystemInfo.m_current_core->m_cpu_num; // TODO: fix
  lock->m_func = __func__;
}

void release_spinlock(__spinlock_t* lock)
{
  lock->m_func = nullptr;
  lock->m_cpu_num = -1;
  __sync_lock_release(lock->m_latch);
}
