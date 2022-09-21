#ifndef __KMALLOC__
#define __KMALLOC__
#include "arch/x86/dev/debug/serial.h"
#include <libc/stddef.h>

#define KHEAP_USED_FLAG 0xFF
#define KHEAP_FREE_FLAG 0x00
#define KHEAP_FUNNY_FLAG 0x69

// TODO: spinlock :clown:

// we are going to need to assert a lot of crap
// so TODO: accuire more info on the assert
static inline void _assert_failure () {
    println("assertion failed in kmalloc!");
    asm volatile("hlt");
    __builtin_unreachable();
}

#define ASSERT(assertion) (assertion ? (void)0 : _assert_failure())

// all heap nodes should be alligned to a page
typedef struct heap_node {
    // size of this entry in bytes
    size_t size;
    // flags for this block
    uint8_t flags;
    // duh
    struct heap_node* next;
    // duh 2x
    struct heap_node* prev;
    // plz no padding ;-;
} __attribute__((packed)) heap_node_t;


void init_kheap();

// our kernel malloc impl
void* __attribute__((malloc)) kmalloc (size_t len);

// our kernel free impl
void kfree (void* addr);

// expand heap by a page
bool try_heap_expand ();

heap_node_t* split_node (heap_node_t* ptr, size_t size);

// TODO: add a wrapper for userspace?

#endif // !__KMALLOC__
