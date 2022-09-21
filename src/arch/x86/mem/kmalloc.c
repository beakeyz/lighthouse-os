#include "kmalloc.h"
#include "arch/x86/kmain.h"
#include "arch/x86/mem/kmem_manager.h"
#include <libc/string.h>
#include <arch/x86/dev/debug/serial.h>
#include <libc/stddef.h>

#define INITIAL_HEAP_SIZE 8192
#define NODE_IDENTIFIER 0xF0CEDA22

static heap_node_t* heap_start_addr;
static size_t heap_available_size;
static size_t heap_used_size;

// this init is post kmem_manager, so expantion will be done using that.
// It might be a good idea though, to heave some initial reserved memory
// BEFORE kmem_manager is initialized, so we can use it earlier
void init_kheap() {
    heap_start_addr = kmem_alloc(INITIAL_HEAP_SIZE);
    heap_start_addr->identifier = NODE_IDENTIFIER;
    heap_start_addr->size = INITIAL_HEAP_SIZE;
    heap_start_addr->flags = KHEAP_FREE_FLAG;
    heap_start_addr->next = NULL;
    heap_start_addr->prev = NULL;
    heap_used_size = 0;
    heap_available_size = INITIAL_HEAP_SIZE;
}

void quick_print_node_sizes() {
    heap_node_t* node = heap_start_addr;
    uintptr_t index = 0;
    while (node) {
        index++;
        
        print("node ");
        print(to_string(index));
        print(": ");
        if (node->flags == KHEAP_FREE_FLAG) {
            print("(free) ");
        }
        println(to_string(node->size));

        node = node->next;
    }
    print("Amount of nodes found: ");
    println(to_string(index));
}

// kmalloc is going to split a node and then return the address of the newly created node + its size to get
// a pointer to the data
void* kmalloc(size_t len) {
    
    heap_node_t* node = heap_start_addr;
    while (node) {
        // TODO: should we also allow allocation when len is equal to the nodesize - structsize?
        if (node->size - sizeof(heap_node_t) > len && node->flags == KHEAP_FREE_FLAG) {
            // yay, our node works =D

            // TODO: for now I'll just assume that at this point, nothing happend to
            // the identifier here. In the future we will handle this correctly
            ASSERT(node->identifier == NODE_IDENTIFIER);

            // now split off a node of the correct size
            heap_node_t* new_node = split_node(node, len);
            if (new_node == nullptr) {
                node = node->next;
                continue;
            }

            new_node->flags = KHEAP_USED_FLAG;
            // for sanity
            new_node->identifier = NODE_IDENTIFIER;

            // TODO: edit global shit
            return (void*) new_node + sizeof(heap_node_t);
        }

        // break early so we can pass the node to the expand func
        if (!node->next) {
            break;
        }
        node = node->next;
    }

    if (try_heap_expand(node)) {
        return kmalloc(len);
    }

    // yikes
    return NULL;
}

void kfree (void* addr) {

    if (addr == NULL) {
        // you fucking little piece of trash, how dare you pass NULL to kfree
        return;
    }

    // first we'll check if we can do this easily
    heap_node_t* node = (heap_node_t*)((uintptr_t)addr - sizeof(heap_node_t));
    ASSERT(verify_identity(node));
    if (node->identifier == NODE_IDENTIFIER) {
        // we can free =D
        if (node->flags == KHEAP_USED_FLAG) {
            node->flags = KHEAP_FREE_FLAG;
            // see if we can merge
            if (!try_merge(node)) {
                // FUCKKKKK
                println("unable to merge nodes =/");
                memset(addr, 0, node->size - sizeof(heap_node_t));
                return;
            }
            println("could merge!");
        }
    }
}

// just expand the heap by one 4KB page
bool try_heap_expand(heap_node_t* last_node) {
    heap_node_t* new_node = kmem_alloc(SMALL_PAGE_SIZE);
    if (new_node) {
        new_node->size = SMALL_PAGE_SIZE;
        new_node->identifier = NODE_IDENTIFIER;
        new_node->flags = KHEAP_FREE_FLAG;
        new_node->next = 0;
        new_node->prev = last_node;
        last_node->next = new_node;
        return try_merge(last_node);
    }
    return false;
}

heap_node_t* split_node(heap_node_t *ptr, size_t size) {
    if (ptr->size <= size) {
        // this is just dumb
        println("[kmalloc:split_node] size is invalid");
        return nullptr;
    }
    
    if (ptr->flags == KHEAP_USED_FLAG) {
        // a node that's completely in use, should not be split
        println("[kmalloc:split_node] tried to split a node that's in use");
        return nullptr;
    }

    // TODO: handle this correctly
    ASSERT(ptr->identifier == NODE_IDENTIFIER);

    // the new node for the allocation
    heap_node_t* new_first_node = ptr;
    // the old free node
    heap_node_t* new_second_node = (heap_node_t*)((uintptr_t)ptr + size + sizeof(heap_node_t));
    new_first_node->identifier = NODE_IDENTIFIER;
    new_second_node->identifier = NODE_IDENTIFIER;
    // now copy the data of the (big) free node over to its smaller counterpart
    new_second_node->size = new_first_node->size - size - sizeof(heap_node_t);
    // This should just be free, but we'll just copy it over anyway
    new_second_node->flags = new_first_node->flags;

    // pointer resolve

    new_second_node->prev = new_first_node;
    new_second_node->next = new_first_node->next;

    // now init the data of our new used node
    new_first_node->next = new_second_node;

    // TODO: these two should prob be asserts
    new_first_node->prev = ptr->prev;
    new_first_node->flags = ptr->flags;
    new_first_node->size = size + sizeof(heap_node_t);

    return new_first_node;
}

bool can_merge(heap_node_t *node1, heap_node_t *node2) {
    if (node1 == NULL || node2 == NULL)
        return false;

    if (node1->flags == KHEAP_FREE_FLAG && node2->flags == KHEAP_FREE_FLAG) {
        if ((node1->prev == node2 && node2->next == node1) || (node1->next == node2 && node2->prev == node1)) {
            return true;
        } 
    }
    return false;
}

// we check for mergeability again, just for sanity =/
// I'd love to make this more efficient, but I'm also too lazy xD
heap_node_t* merge_node_with_next (heap_node_t* ptr) {
    if (can_merge(ptr, ptr->next)) {
        ptr->size += ptr->next->size;
        heap_node_t* next_next = ptr->next->next;
        memset(ptr->next, 0, ptr->next->size);

        ptr->next = next_next;
        return ptr;
    }
    return nullptr;
}
heap_node_t* merge_node_with_prev (heap_node_t* ptr) {
    if (can_merge(ptr, ptr->prev)) {
        ptr->prev->size += ptr->size;
        heap_node_t* prev = ptr->prev;
        prev->next = ptr->next;
        memset(ptr, 0, ptr->size);
        return prev;
    }
    return nullptr;
}
heap_node_t* merge_nodes (heap_node_t* ptr1, heap_node_t* ptr2) {
    if (can_merge(ptr1, ptr2)) {
        // Because they passed the merge check we can do this =D
        // sm -> ptr1 -> ptr2 -> sm
        if (ptr1->next == ptr2) {
            return merge_node_with_next(ptr1);
        }
        // ptr2 -> ptr1 -> sm
        return merge_node_with_prev(ptr1);
    }
    return nullptr;
}

bool try_merge(heap_node_t *node) {
    heap_node_t* merged_node = merge_nodes(node, node->next);
    if (merged_node == nullptr) {
        merged_node = merge_nodes(node, node->prev);
    }

    if (merged_node == nullptr) {
        return false;
    }

    // we don't really care about if the recursive merges fail
    try_merge(node);
    return true;
}

// for if we want to handle sm
bool verify_identity(heap_node_t *node) {
    return node->identifier == NODE_IDENTIFIER;
}
