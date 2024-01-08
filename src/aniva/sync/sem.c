#include "sem.h"
#include "libk/data/queue.h"
#include "libk/flow/error.h"
#include "mem/heap.h"
#include "sync/atomic_ptr.h"
#include "sync/spinlock.h"

/*
 * Semaphores are pretty much just spinlocks with a little buffer lmao
 */
typedef struct semaphore {
  uint32_t limit;
  atomic_ptr_t value;
  queue_t* waiters;
  spinlock_t* wait_lock;
} semaphore_t;

struct semaphore* create_semaphore(uint32_t limit, uint32_t value, uint32_t wait_capacity)
{
  semaphore_t* ret;

  ret = kmalloc(sizeof(*ret));

  if (!ret)
    return nullptr;

  ret->limit = limit;
  ret->waiters = create_queue(wait_capacity);
  ret->wait_lock = create_spinlock();
  init_atomic_ptr(&ret->value, value);

  return ret;
}

void destroy_semaphore(struct semaphore* sem)
{
  destroy_spinlock(sem->wait_lock);
  destroy_queue(sem->waiters, false);
  kfree(sem);
}

int sem_enqueue_waiter(struct semaphore* sem, void* waiter)
{
  kernel_panic("TODO: queue a waiter for a semaphore");
  spinlock_lock(sem->wait_lock);
  return 0;
}

int sem_dequeue_waiter(struct semaphore* sem)
{
  kernel_panic("TODO: dequeue a waiter for a semaphore");
  spinlock_unlock(sem->wait_lock);
  return 0;
}

/*!
 * @brief: Semaphore 'down' function
 *
 * 
 */
int sem_wait(struct semaphore* sem, void* waiter)
{
  int error;
  int64_t val;

  error = 0;
  val = atomic_ptr_read(&sem->value);

  if (val <= 0)
    error = sem_enqueue_waiter(sem, waiter);

  /* Always decrement the semaphore value */
  atomic_ptr_write(&sem->value, val-1);
  return error;
}

int sem_post(struct semaphore* sem)
{
  int error;
  int64_t val;

  val = atomic_ptr_read(&sem->value);
  error = 0;

  /* There are waiters. Dequeue them */
  if (val <= 0)
    error = sem_dequeue_waiter(sem);

  /* Always increment the semaphore value */
  atomic_ptr_write(&sem->value, val+1);
  return error;
}

bool semaphore_is_binary(struct semaphore* sem)
{
  return (sem->limit == 1);
}
