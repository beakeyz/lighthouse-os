#ifndef __LIGHT_SPINLOCK__
#define __LIGHT_SPINLOCK__
#include "kmain.h"
#include <libk/stddef.h>

typedef volatile struct {
  volatile int m_latch[1];
  int m_cpu_num;
  const char* m_func;
} __spinlock_t;

typedef struct {
  __spinlock_t m_lock;
  Processor_t* m_processor;
  proc_t* m_proc;
  thread_t* m_thread;

} spinlock_t;

spinlock_t* init_spinlock();
void lock_spinlock(spinlock_t* lock);
void unlock_spinlock(spinlock_t* lock);
bool is_spinlock_locked(spinlock_t* lock);

/* __spinlock_t */
__spinlock_t __init_spinlock();

// arch specific
void aquire_spinlock(__spinlock_t* lock);
void release_spinlock(__spinlock_t* lock);

#endif // !__LIGHT_SPINLOCK__
