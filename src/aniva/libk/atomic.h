#ifndef __ANIVA_ATOMIC__
#define __ANIVA_ATOMIC__

#include "libk/stddef.h"

// serenityOS
typedef enum {
    memory_order_relaxed = __ATOMIC_RELAXED,
    memory_order_consume = __ATOMIC_CONSUME,
    memory_order_acquire = __ATOMIC_ACQUIRE,
    memory_order_release = __ATOMIC_RELEASE,
    memory_order_acq_rel = __ATOMIC_ACQ_REL,
    memory_order_seq_cst = __ATOMIC_SEQ_CST
} MemOrder;

static ALWAYS_INLINE void _atomic_store(volatile void** var, void* d, MemOrder order)
{
    __atomic_store_n(var, d, order);
}

static ALWAYS_INLINE void _atomic_store_alt(volatile uintptr_t* var, uintptr_t d, MemOrder order)
{
    __atomic_store_n(var, d, order);
}

static ALWAYS_INLINE void* _atomic_load(volatile void** var, MemOrder order)
{
    return __atomic_load_n(&var, order);
}

static ALWAYS_INLINE uintptr_t _atomic_load_alt(volatile uintptr_t* var, MemOrder order)
{
    return __atomic_load_n(var, order);
}

//
// Defaults
//
// x86 memory barier instructions
//
// NOTE: If (Or rather when) we want to port this system
// to multiple architectures, we need to find the arch-specific instructions
//

static ALWAYS_INLINE void mem_barier_read()
{
    /* Load fence */
    asm volatile("lfence" : : : "memory");
}

static ALWAYS_INLINE void mem_barier_write()
{
    /* Store fence */
    asm volatile("sfence" : : : "memory");
}

static ALWAYS_INLINE void mem_barier_full()
{
    /* Memory fence */
    asm volatile("mfence" : : : "memory");
}

static ALWAYS_INLINE void atomicStore(volatile void** var, void* d)
{
    _atomic_store(var, d, memory_order_seq_cst);
}

static ALWAYS_INLINE void atomicStore_alt(volatile uintptr_t* var, uintptr_t d)
{
    _atomic_store_alt(var, d, memory_order_seq_cst);
}

static ALWAYS_INLINE void* atomicLoad(volatile void** var)
{
    return _atomic_load(var, memory_order_seq_cst);
}

static ALWAYS_INLINE uintptr_t atomicLoad_alt(volatile uintptr_t* var)
{
    return _atomic_load_alt(var, memory_order_seq_cst);
}

#endif // !__ANIVA_ATOMIC__
