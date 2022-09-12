#include "kmalloc.h"
#include "arch/x86/kmain.h"
#include "arch/x86/mem/kmem_bitmap.h"
#include <libc/string.h>

// 2MB initial heap
__attribute__((section(".heap"))) static uint8_t _initial_heap_mem[2*1024];

static kmalloc_data_t g_kmalloc_data;
static kmem_bitmap_t* kmem_manager_bitmap_ptr;

static size_t get_num_chunks (size_t mem_size);

void init_kmalloc(kmem_bitmap_t* bitmap) {
    memset(_initial_heap_mem, 0, sizeof(_initial_heap_mem));

    // TODO: works?
    kmem_manager_bitmap_ptr = bitmap;

    g_kmalloc_data.heap_addr = _initial_heap_mem + PAGE_SIZE;
    g_kmalloc_data.allocated_chunks = 0;
    g_kmalloc_data.total_chunks = get_num_chunks(sizeof(_initial_heap_mem) - PAGE_SIZE);

}

// TODO: manage g_kmalloc_data with these functions and use this to (de)allocate the 
// correct amount of space
void* kmalloc(size_t len) {
    return NULL;
}

void* kfree (void* addr, size_t len) {
    return NULL;
}

static size_t get_num_chunks (size_t mem_size) {
    return (sizeof(uint8_t) * mem_size) / (sizeof(uint8_t) * CHUNK_SIZE + 1);
}

