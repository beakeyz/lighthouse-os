#ifndef __ANIVA_SPINLOCK__
#define __ANIVA_SPINLOCK__

#include <libk/stddef.h>

struct processor;

#define SPINLOCK_FLAG_LOCKED 0x00000001
#define SPINLOCK_FLAG_HAD_INTERRUPTS 0x00000002
#define SPINLOCK_FLAG_SOFT 0x00000004

typedef struct spinlock {
    volatile int m_latch[1];
    u32 m_flags;
    struct processor* m_processor;
} spinlock_t;

spinlock_t* create_spinlock(u32 flags);
void destroy_spinlock(spinlock_t* lock);

void spinlock_lock(spinlock_t* lock);
void spinlock_unlock(spinlock_t* lock);
bool spinlock_is_locked(spinlock_t* lock);

#endif // !__ANIVA_SPINLOCK__
