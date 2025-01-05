#ifndef __ANIVA_SYNC_MUTEX__
#define __ANIVA_SYNC_MUTEX__
#include "libk/data/queue.h"
#include "proc/thread.h"
#include "sync/spinlock.h"
#include <libk/stddef.h>

/* This mutex is allocated on the kernel heap */
#define MUTEX_FLAG_ALLOCATED 0x00000001

typedef struct mutex {
    const char* m_name;
    spinlock_t* m_lock;
    queue_t* m_waiters;
    thread_t* m_lock_holder;
    uint32_t m_lock_depth;
    uint32_t flags;
} mutex_t;

/*
 * Allocate and initialize a mutex
 */
mutex_t* create_mutex(uint8_t flags);
void init_mutex(mutex_t* lock, uint8_t flags);
void clear_mutex(mutex_t* mutex);

/*
 * Deallocate and uninitialize the mutex
 */
void destroy_mutex(mutex_t* mutex);

/*
 * Lock the mutex, which makes any further locks by other threads
 * result in a blocking queue
 */
void mutex_lock(mutex_t* mutex);

/*
 * Unlock the mutex, which passes it onto the next holder
 */
void mutex_unlock(mutex_t* mutex);

void mutex_release(mutex_t* mutex, thread_t* thread);

/*
 * Checks whether the mutex is held
 */
bool mutex_is_locked(mutex_t* mutex);

/*
 * Checks whether the mutex is held by the current thread
 */
bool mutex_is_locked_by_current_thread(mutex_t* mutex);

#endif // !__ANIVA_SYNC_MUTEX__
