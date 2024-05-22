#ifndef __ANIVA_SPINLOCK__
#define __ANIVA_SPINLOCK__

#include <libk/stddef.h>

struct processor;

/* Prototype the spinlock struct */
typedef struct spinlock spinlock_t;

spinlock_t* create_spinlock();
void destroy_spinlock(spinlock_t* lock);

void spinlock_lock(spinlock_t* lock);
void spinlock_unlock(spinlock_t* lock);

bool spinlock_is_locked(spinlock_t* lock);

#endif // !__ANIVA_SPINLOCK__
