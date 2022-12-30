#include "spinlock.h"
#include "mem/kmalloc.h"

spinlock_t init_spinlock() {
  spinlock_t ret = {
    .m_latch[0] = 0,
    .m_cpu_num = -1,
    .m_func = nullptr
  };
  return ret;
}

void aquire_spinlock(spinlock_t* lock)
{
  while (__sync_lock_test_and_set(lock->m_latch, 0x01));
  lock->m_cpu_num = g_GlobalSystemInfo.m_bsp_processor.m_cpu_num; // TODO: fix
  lock->m_func = __func__;
}

void release_spinlock(spinlock_t* lock)
{
  lock->m_func = nullptr;
  lock->m_cpu_num = -1;
  __sync_lock_release(lock->m_latch);
}
