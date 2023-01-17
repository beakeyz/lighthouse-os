#ifndef __ANIVA_SPINLOCK__
#define __ANIVA_SPINLOCK__
#include <libk/stddef.h>

typedef volatile struct {
  volatile int m_latch[1];
  int m_cpu_num;
  const char* m_func;
} __spinlock_t;

typedef struct {
  __spinlock_t m_lock;
  // TODO:
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

#endif // !__ANIVA_SPINLOCK__
