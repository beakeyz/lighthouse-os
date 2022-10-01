// For now this will be a super simple pool allocator. In order to increase speed and 
// flexibility, we will add a slab alloctor in the future for smaller allocations.

#ifndef __KMALLOC__
#define __KMALLOC__
#include "arch/x86/dev/debug/serial.h"
#include <libk/stddef.h>

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

// FIXME: this design may have some security issues =(
// all heap nodes should be alligned to a page
typedef struct heap_node {
    // size of this entry in bytes
    size_t size;
    // used to validate a node pointer
    uintptr_t identifier;
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
bool try_heap_expand (heap_node_t* last_node);

heap_node_t* split_node (heap_node_t* ptr, size_t size);
heap_node_t* merge_node_with_next (heap_node_t* ptr);
heap_node_t* merge_node_with_prev (heap_node_t* ptr);
// here we will check if they are mergable (aka next to eachother)
heap_node_t* merge_nodes (heap_node_t* ptr1, heap_node_t* ptr2);
bool try_merge (heap_node_t* node);

bool can_merge (heap_node_t* node1, heap_node_t* node2);

bool verify_identity (heap_node_t* node);

heap_node_t* copy_pointers (heap_node_t* from, heap_node_t* to);

// TODO: remove
void quick_print_node_sizes ();

// TODO: add a wrapper for userspace?
#endif // !__KMALLOC__
