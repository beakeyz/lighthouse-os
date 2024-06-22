#include "spinlock.h"
#include "irq/interrupts.h"
#include "libk/flow/error.h"
#include "libk/stddef.h"
#include "sync/atomic_ptr.h"
#include "system/processor/processor.h"
#include <libk/string.h>
#include <mem/heap.h>

// FIXME: make this wrapper actually useful
spinlock_t* create_spinlock(u32 flags)
{
    spinlock_t* lock;

    lock = kmalloc(sizeof(spinlock_t));

    if (!lock)
        return nullptr;

    memset(lock, 0, sizeof(*lock));

    /* Make sure we know we're on the heap */
    lock->m_flags = flags;

    return lock;
}

void destroy_spinlock(spinlock_t* lock)
{
    if (!lock)
        return;

    // if (spinlock_is_locked(lock))
    // spinlock_unlock(lock);

    kfree(lock);
}

static inline int aquire_spinlock(spinlock_t* lock)
{
    bool has_interrupts;

    if (!lock)
        return -1;

    has_interrupts = interrupts_are_enabled();

    /* Disable interrupts */
    disable_interrupts();

    /* Mark our interrupt state */
    if (has_interrupts)
        lock->m_flags |= SPINLOCK_FLAG_HAD_INTERRUPTS;

    /* Take the lock */
    while (__sync_lock_test_and_set(lock->m_latch, 0x01))
        ;

    lock->m_flags |= SPINLOCK_FLAG_LOCKED;
    return 0;
}

static inline int release_spinlock(spinlock_t* lock)
{
    /* Clear local flag */
    lock->m_flags &= ~SPINLOCK_FLAG_LOCKED;
    lock->m_processor = nullptr;

    /* Release the latch */
    __sync_lock_release(lock->m_latch);

    /* No interrupts enabled? Just return, we trust they get enabled somewhere down the line */
    if ((lock->m_flags & SPINLOCK_FLAG_HAD_INTERRUPTS) != SPINLOCK_FLAG_HAD_INTERRUPTS)
        return 0;

    /* If interrupts were enabled when we locked the bastard, enable them now again */
    lock->m_flags &= ~SPINLOCK_FLAG_HAD_INTERRUPTS;
    enable_interrupts();
    return 0;
}

/*
 * FIXME: should we limit spinlock usage in IRQs?
 * could they pose a risk for potential deadlocks?
 */
void spinlock_lock(spinlock_t* lock)
{
    uintptr_t cpu_lock_level;
    processor_t* c_cpu;

    if (!lock)
        return;

    /* Get the current CPU */
    c_cpu = get_current_processor();

    /* Idk */
    ASSERT_MSG(c_cpu, "Tried to lock a spinlock without a CPU! (huh?)");

    if (!c_cpu->m_locked_level)
        return;

    ASSERT_MSG(c_cpu->m_irq_depth == 0, "Can't lock a spinlock inside an IRQ!");
    /* Rarely fails */
    ASSERT_MSG(aquire_spinlock(lock) == 0, "Failed to aquire the spinlock!");

    /* Store the current CPU of this spinlock */
    lock->m_processor = c_cpu;

    /* Increase the processors locked level */
    cpu_lock_level = atomic_ptr_read(lock->m_processor->m_locked_level) + 1;
    atomic_ptr_write(lock->m_processor->m_locked_level, cpu_lock_level);
}

/**
 * @brief: Release a spinlock
 */
void spinlock_unlock(spinlock_t* lock)
{
    uintptr_t cpu_lock_level;
    processor_t* c_cpu;

    if (!lock)
        return;

    c_cpu = get_current_processor();

    if (!c_cpu->m_locked_level)
        return;

    cpu_lock_level = atomic_ptr_read(lock->m_processor->m_locked_level);
    ASSERT_MSG(cpu_lock_level > 0, "unlocking spinlock while having a m_locked_level of 0!");

    atomic_ptr_write(lock->m_processor->m_locked_level, cpu_lock_level - 1);

    lock->m_processor = nullptr;

    release_spinlock(lock);
}

/**
 * @brief: Check if a spinlock is locked
 *
 * This is a dangerous call, especially during SMP, because the lock can
 * get locked/unlocked at any time
 *
 * @lock: The lock lmao
 */
bool spinlock_is_locked(spinlock_t* lock)
{
    return (lock->m_flags & SPINLOCK_FLAG_LOCKED) == SPINLOCK_FLAG_LOCKED;
}
