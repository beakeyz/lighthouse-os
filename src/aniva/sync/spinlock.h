#ifndef __ANIVA_SPINLOCK__
#define __ANIVA_SPINLOCK__
#include "sync/atomic_ptr.h"
#include <libk/stddef.h>

struct processor;

typedef volatile struct {
  volatile int m_latch[1];
  int m_cpu_num;
  const char* m_func;
} __spinlock_t;

typedef struct spinlock {
  __spinlock_t m_lock;
  struct processor* m_processor;
  atomic_ptr_t* m_is_locked;
  // TODO:
} spinlock_t;

spinlock_t* create_spinlock();
void init_spinlock(spinlock_t* lock);
void destroy_spinlock(spinlock_t* lock);

void spinlock_lock(spinlock_t* lock);

void spinlock_unlock(spinlock_t* lock);

bool spinlock_is_locked(spinlock_t* lock);

#endif // !__ANIVA_SPINLOCK__
