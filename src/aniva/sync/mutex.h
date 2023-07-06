#ifndef __ANIVA_SYNC_MUTEX__
#define __ANIVA_SYNC_MUTEX__
#include "libk/data/queue.h"
#include "proc/thread.h"
#include "sync/spinlock.h"
#include <libk/stddef.h>

/*
 * When passing this flag to the mutex on creation, 
 * we imply that the mutex immediately gets locked after 
 * creation
 */
#define MUTEX_FLAG_IS_HELD                  (1<<0)

/*
 * TODO: implement
 */
#define MUTEX_FLAG_IS_SHARED                (1<<1)

typedef struct mutex {
  uint8_t m_mutex_flags;
  size_t m_lock_depth;

  thread_t* m_lock_holder;

  queue_t* m_waiters;

  spinlock_t* m_lock;
} mutex_t;

/*
 * Allocate and initialize a mutex
 */
mutex_t* create_mutex(uint8_t flags);

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

/*
 * Checks whether the mutex is held
 */
bool mutex_is_locked(mutex_t* mutex);

/*
 * Checks whether the mutex is held by the current thread
 */
bool mutex_is_locked_by_current_thread(mutex_t* mutex);

#endif // !__ANIVA_SYNC_MUTEX__
