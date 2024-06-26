#include "sem.h"
#include "libk/data/queue.h"
#include "mem/heap.h"
#include "proc/thread.h"
#include "sched/scheduler.h"
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
    ret->wait_lock = create_spinlock(NULL);
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
    thread_t* this_thread;
    //kernel_panic("TODO: queue a waiter for a semaphore");
    //spinlock_lock(sem->wait_lock);
    
    this_thread = get_current_scheduling_thread();
    
    if (!this_thread)
        return -1;

    queue_enqueue(sem->waiters, this_thread);

    spinlock_unlock(sem->wait_lock);

    thread_block(this_thread);
    
    /* Lock the spinlock again once we get unblocked */
    spinlock_lock(sem->wait_lock);

    return 0;
}

int sem_dequeue_waiter(struct semaphore* sem)
{
    thread_t* target_thread;

    target_thread = queue_dequeue(sem->waiters);

    if (!target_thread)
      return -1;

    thread_unblock(target_thread);
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

    spinlock_lock(sem->wait_lock);

    val = atomic_ptr_read(&sem->value) - 1;

    /* Always decrement the semaphore value */
    atomic_ptr_write(&sem->value, val);

    if (val < 0)
        error = sem_enqueue_waiter(sem, waiter);

    spinlock_unlock(sem->wait_lock);

    return error;
}

int sem_post(struct semaphore* sem)
{
    int error;
    int64_t val;

    error = 0;

    spinlock_lock(sem->wait_lock);

    /* Post the semaphore */
    val = atomic_ptr_read(&sem->value) + 1;

    /* Always increment the semaphore value */
    atomic_ptr_write(&sem->value, val);

    /* There are waiters. Dequeue them */
    if (val <= 0)
        error = sem_dequeue_waiter(sem);

    spinlock_unlock(sem->wait_lock);

    return error;
}

/**
 * @brief: Resets the semaphore to zero if there are waiters
 */
int sem_drain(struct semaphore* sem)
{
    int error;
    int64_t val;

    error = 0;

    spinlock_lock(sem->wait_lock);

    val = atomic_ptr_read(&sem->value);

    if (val)
      goto unlock_and_exit;

    val++;

    do {
        /* Always increment the semaphore value */
        atomic_ptr_write(&sem->value, val);

        /* There are waiters. Dequeue them */
        if (val <= 0)
            error = sem_dequeue_waiter(sem);

        val++;
    } while (val <= 0);

unlock_and_exit:
    spinlock_unlock(sem->wait_lock);
    return error;
}

bool semaphore_is_binary(struct semaphore* sem)
{
    return (sem->limit == 1);
}
