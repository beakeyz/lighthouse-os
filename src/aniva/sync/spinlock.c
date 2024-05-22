#include "spinlock.h"
#include "libk/flow/error.h"
#include "libk/stddef.h"
#include "sched/scheduler.h"
#include "sync/atomic_ptr.h"
#include "system/processor/processor.h"
#include <mem/heap.h>
#include <libk/string.h>

typedef struct spinlock {
  volatile int m_latch[1];
  int m_cpu_num;
  struct processor* m_processor;
} spinlock_t;

// FIXME: make this wrapper actually useful
spinlock_t* create_spinlock() 
{
  spinlock_t* lock;

  lock = kmalloc(sizeof(spinlock_t));

  if (!lock)
    return nullptr;

  memset(lock, 0, sizeof(*lock));

  lock->m_cpu_num = -1;

  return lock;
}

void destroy_spinlock(spinlock_t* lock)
{
  kfree(lock);
}

static inline void __aquire_spinlock(spinlock_t* lock, processor_t* c_cpu)
{
  if (!lock || !c_cpu)
    return;

  while (__sync_lock_test_and_set(lock->m_latch, 0x01)) {
    ASSERT_MSG(c_cpu->m_irq_depth == 0, "Can't block on a spinlock from within an IRQ!");

    /* FIXME: should we? */
    scheduler_yield();
  }

  lock->m_cpu_num = (int)c_cpu->m_cpu_num;
}

static inline void __release_spinlock(spinlock_t* lock)
{
  lock->m_cpu_num = -1;
  __sync_lock_release(lock->m_latch);
}

/*
 * FIXME: should we limit spinlock usage in IRQs? 
 * could they pose a risk for potential deadlocks?
 */
void spinlock_lock(spinlock_t* lock) 
{
  processor_t* c_cpu;
  
  if (!lock)
    return;

  c_cpu = get_current_processor();

  if (!c_cpu->m_locked_level)
    return;

  /* Try to aquire the latch */
  __aquire_spinlock(lock, c_cpu);

  lock->m_processor = c_cpu;

  uintptr_t j = atomic_ptr_read(lock->m_processor->m_locked_level);
  atomic_ptr_write(lock->m_processor->m_locked_level, j+1);
}

void spinlock_unlock(spinlock_t* lock) 
{
  uintptr_t cpu_locked_level;
  processor_t* c_cpu;

  if (!lock)
    return;

  /* Not locked, hihi */
  if (!spinlock_is_locked(lock))
    return;

  c_cpu = get_current_processor();

  if (!c_cpu->m_locked_level)
    return;

  /* This would be VERY bad */
  ASSERT_MSG(lock->m_cpu_num == c_cpu->m_cpu_num, "Tried to unlock a spinlock from a different CPU");

  cpu_locked_level = atomic_ptr_read(lock->m_processor->m_locked_level);

  if (!cpu_locked_level)
    return;

  atomic_ptr_write(lock->m_processor->m_locked_level, cpu_locked_level-1);

  lock->m_processor = nullptr;

  __release_spinlock(lock);
}

bool spinlock_is_locked(spinlock_t* lock) 
{
  return (lock->m_cpu_num != -1);
}
