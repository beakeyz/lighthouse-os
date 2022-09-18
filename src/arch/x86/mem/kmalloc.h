#ifndef __KMALLOC__
#define __KMALLOC__
#include "arch/x86/dev/debug/serial.h"
#include <libc/stddef.h>

// TODO: spinlock :clown:

// we are going to need to assert a lot of crap
// so TODO: accuire more info on the assert
static inline void _assert_failure () {
    println("assertion failed in kmalloc!");
    asm volatile("hlt");
    __builtin_unreachable();
}

#define ASSERT(assertion) (assertion ? (void)0 : _assert_failure())

typedef struct kmalloc_slab_block {
    
} kmalloc_slab_block_t ;

// our kernel malloc impl
void* __attribute__((malloc)) kmalloc (size_t len);

// our kernel free impl
void kfree (void* addr);

// TODO: add a wrapper for userspace?

#endif // !__KMALLOC__
