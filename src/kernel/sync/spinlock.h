#ifndef __LIGHT_SPINLOCK__
#define __LIGHT_SPINLOCK__
#include "kmain.h"
#include <libk/stddef.h>

typedef volatile struct {
  volatile int m_latch[1];
  int m_cpu_num;
  const char* m_func;
} spinlock_t;

spinlock_t init_spinlock();

// arch specific
void aquire_spinlock(spinlock_t* lock);
void release_spinlock(spinlock_t* lock);

#endif // !__LIGHT_SPINLOCK__
