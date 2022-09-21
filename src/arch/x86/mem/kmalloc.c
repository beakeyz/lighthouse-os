#include "kmalloc.h"
#include "arch/x86/kmain.h"
#include "arch/x86/mem/kmem_manager.h"
#include <libc/string.h>
#include <arch/x86/dev/debug/serial.h>
#include <libc/stddef.h>

#define INITIAL_HEAP_SIZE 8192

static heap_node_t* heap_start_addr;
static size_t heap_available_size;
static size_t heap_used_size;
static heap_node_t* current_heap_node;

void init_kheap() {
    heap_start_addr = kmem_alloc(INITIAL_HEAP_SIZE);

    heap_start_addr->size = INITIAL_HEAP_SIZE;
    heap_start_addr->flags = KHEAP_FREE_FLAG;
    heap_start_addr->next = NULL;
    heap_start_addr->prev = NULL;
    heap_used_size = 0;
    heap_available_size = INITIAL_HEAP_SIZE;
}

// kmalloc is going to split a node and then return the address of the newly created node + its size to get
// a pointer to the data
void* kmalloc(size_t len) {
    
    heap_node_t* node = heap_start_addr;
    while (node) {
        if (node->size <= len) {
            // yay, our node works =D

            // now split off a node of the correct size
            heap_node_t* new_node = split_node(node, len);
            // TODO: edit global shit
            return (void*) new_node + sizeof(heap_node_t);
        }
        node = node->next;
    }

    ASSERT(len);

    if (len) {}
    return NULL;
}

void kfree (void* addr) {

    if (addr == NULL) {
        // you fucking little piece of trash, how dare you pass NULL to kfree
        return;
    }

    if (addr) {}
}

bool try_heap_expand() {

    // fuck man, now we die =/
    return false;
}

heap_node_t* split_node(heap_node_t *ptr, size_t size) {
    if (ptr->size >= size) {
        // this is just dumb
        println("[kmalloc:split_node] size is invalid");
        return nullptr;
    }
    
    if (ptr->flags & KHEAP_USED_FLAG) {
        // a node that's completely in use, should not be split
        println("[kmalloc:split_node] tried to split a node that's in use");
        return nullptr;
    }

    // reach the COMPLETE end of the block, by getting the addr and size
    heap_node_t* new_addr = (heap_node_t*)(ptr + ptr->size) - size;
    new_addr->flags = KHEAP_USED_FLAG;
    new_addr->size = size;

    ASSERT ((new_addr + size) == (ptr + ptr->size));
    
    new_addr->prev = ptr;
    new_addr->next = ptr->next;
    ptr->next = new_addr;
    // we dont care about the previous node of the ptr lmao
    return new_addr;
}
