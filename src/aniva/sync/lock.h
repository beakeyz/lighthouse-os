#ifndef __ANIVA_SYNC_LOCK__
#define __ANIVA_SYNC_LOCK__

/*
 * Universal lock object
 *
 * This should be used to track locking statuses of different system entities (Like processes or drivers)
 *
 * TODO: how?
 */

struct mutex;
struct spinlock;

enum LOCK_TYPE {
  LOCK_TYPE_SPINLOCK = 0,
  LOCK_TYPE_MUTEX,
};

/*
 * Generic lock object
 *
 * This object has no ownership of the underlying locks
 * destroying this object won't result in the locks backend
 * struct being destroyed
 */
typedef struct lock {
  union {
    struct mutex* mutex;
    struct spinlock* spinlock;
  } l;
  enum LOCK_TYPE type;
} lock_t;

lock_t* create_lock(enum LOCK_TYPE type, void* lock);
void destroy_lock(lock_t* lock);

typedef struct lock_list {
  
} lock_list_t;

#endif // !__ANIVA_SYNC_LOCK__
