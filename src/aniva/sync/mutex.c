#include "mutex.h"
#include "libk/data/queue.h"
#include "libk/flow/error.h"
#include "mem/heap.h"
#include "proc/thread.h"
#include "sched/scheduler.h"
#include "sync/spinlock.h"
#include "system/processor/processor.h"

/*
 * Wrapper that unblocks the threads that tried to
 * take the mutex while it was locked
 */
static int __mutex_handle_unblock(mutex_t* mutex);

mutex_t* create_mutex(uint8_t flags)
{
    mutex_t* ret;

    ret = kmalloc(sizeof(mutex_t));

    if (!ret)
        return nullptr;

    memset(ret, 0, sizeof(*ret));

    ret->m_waiters = create_limitless_queue();
    ret->m_lock = create_spinlock();
    ret->m_lock_holder = nullptr;

    return ret;
}

void init_mutex(mutex_t* lock, uint8_t flags)
{
    memset(lock, 0, sizeof(*lock));

    lock->m_waiters = create_limitless_queue();
    lock->m_lock = create_spinlock();
    lock->m_lock_holder = nullptr;
}

void clear_mutex(mutex_t* mutex)
{
    thread_t* waiter;

    if (!mutex || !mutex->m_waiters)
        return;

    /*
     * FIXME: This can get nasty if there are other threads
     * waiting to do stuff with this mutex, because the
     * object that this mutex protects is then unprotected for a time
     */
    waiter = queue_dequeue(mutex->m_waiters);

    while (waiter) {

        kernel_panic("TODO: figure out what to do with waiter threads on a mutex when the mutex is destroyed");
        // waiter->m_current_state = DYING;
        // thread_unblock(waiter);

        waiter = queue_dequeue(mutex->m_waiters);
    }

    destroy_queue(mutex->m_waiters, false);
}

void destroy_mutex(mutex_t* mutex)
{
    if (!mutex)
        return;

    thread_unregister_mutex(mutex->m_lock_holder, mutex);

    clear_mutex(mutex);

    destroy_spinlock(mutex->m_lock);
    kfree(mutex);
}

void mutex_lock(mutex_t* mutex)
{
    thread_t* current_thread;
    ASSERT_MSG(mutex, "Tried to lock a mutex that has not been initialized");

    current_thread = get_current_scheduling_thread();

    if (!current_thread)
        return;

retry_lock:
    spinlock_lock(mutex->m_lock);

    if (!mutex->m_lock_holder)
        goto do_lock;

    /*
     * This also works outside the scheduler context, since we won't block threads ever
     * if they are null
     */
    // ASSERT_MSG(current_thread != mutex->m_lock_holder, "Tried to lock the same mutex twice!");
    if (current_thread == mutex->m_lock_holder)
        goto skip_lock_register;

    /* We may lock a mutex from within a irq, but we cant block on it */
    ASSERT_MSG(get_current_processor()->m_irq_depth == 0, "Can't block on a mutex from within an IRQ!");

    // block current thread
    queue_enqueue(mutex->m_waiters, current_thread);

    // NOTE: when we block this thread, it returns executing here after it gets unblocked by mutex_unlock,
    // since we just yield to the scheduler when we're blocked

    spinlock_unlock(mutex->m_lock);

    thread_block(current_thread);

    goto retry_lock;

    /*
    spinlock_lock(mutex->m_lock);

    ASSERT_MSG(mutex->m_lock_depth == 0, "Mutex was not unlocked after thread got unblocked!");
    */

do_lock:
    thread_register_mutex(current_thread, mutex);

    mutex->m_lock_holder = current_thread;

skip_lock_register:
    mutex->m_lock_depth++;
    spinlock_unlock(mutex->m_lock);
}

/*!
 * @brief: Unlocks a mutex
 *
 * @returns: True if an unblock happend, False otherwise
 */
static bool mutex_unlock_locked(mutex_t* mutex, bool should_unregister)
{
    thread_t* c_thread;

    c_thread = get_current_scheduling_thread();

    if (!c_thread)
        return false;

    ASSERT_MSG(mutex && mutex->m_lock, "Tried to unlock a mutex that has not been initialized");

    ASSERT_MSG(mutex->m_lock_depth--, "Tried to unlock a mutex while it was already unlocked!");

    if (!mutex->m_lock_depth) {

        if (should_unregister)
            thread_unregister_mutex(mutex->m_lock_holder, mutex);

        /* Unblock */
        if (__mutex_handle_unblock(mutex) == 0)
            return true;
    }

    return false;
}

void mutex_unlock(mutex_t* mutex)
{
    bool did_unblock;

    /* No need */
    if (!get_current_scheduling_thread())
        return;

    spinlock_lock(mutex->m_lock);

    did_unblock = mutex_unlock_locked(mutex, true);

    /* Only unlock if there was no unblock */
    if (!did_unblock)
        spinlock_unlock(mutex->m_lock);
}

/*!
 * @brief: Releases @thread from @mutex
 *
 * If @thread holds @mutex, we will call mutex_unlock. Otherwise we try to remove @thread
 * from the waiters queue, if it's in it.
 */
void mutex_release(mutex_t* mutex, thread_t* thread)
{
    bool did_unblock;

    spinlock_lock(mutex->m_lock);

    did_unblock = false;

    if (mutex->m_lock_holder == thread)
        did_unblock = mutex_unlock_locked(mutex, false);
    else
        (void)queue_remove(mutex->m_waiters, thread);

    if (!did_unblock)
        spinlock_unlock(mutex->m_lock);
}

// FIXME: inline?
bool mutex_is_locked(mutex_t* mutex)
{
    // No mutex means no lock =/
    if (!mutex || !mutex->m_lock_holder || !mutex->m_lock)
        return false;

    return (mutex->m_lock_depth > 0);
}

// FIXME: inline?
bool mutex_is_locked_by_current_thread(mutex_t* mutex)
{
    return (mutex->m_lock_holder == get_current_scheduling_thread());
}

/*!
 * @brief: Handle the cycling of waiters
 *
 * NOTE: the caller must have the mutexes spinlock held
 */
static int __mutex_handle_unblock(mutex_t* mutex)
{
    thread_t* current_thread;
    thread_t* next_holder;

    if (!mutex)
        return -1;

    /* Preemptive reset of the holder field */
    mutex->m_lock_holder = nullptr;

    current_thread = get_current_scheduling_thread();
    next_holder = queue_dequeue(mutex->m_waiters);

    if (!current_thread || !next_holder)
        return -1;

    pause_scheduler();

    ASSERT_MSG(next_holder != current_thread, "Next thread to hold mutex is also the current thread!");

    /* Set the next holder */
    mutex->m_lock_holder = next_holder;

    /* Release the mutext */
    spinlock_unlock(mutex->m_lock);

    /* Mark the thread unblocked */
    thread_unblock(next_holder);

    /* Release the scheduler */
    resume_scheduler();

    /* Yield to the scheduler */
    scheduler_yield();

    return 0;
}
