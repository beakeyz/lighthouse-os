#include "kmalloc.h"
#include "arch/x86/kmain.h"
#include "arch/x86/mem/kmem_bitmap.h"
#include <libc/string.h>
#include <arch/x86/dev/debug/serial.h>


static kmalloc_data_t g_kmalloc_data;
static kmem_bitmap_t* kmem_manager_bitmap_ptr;

static size_t get_num_chunks (size_t mem_size);

void init_kmalloc(kmem_bitmap_t* bitmap, uint8_t* heap_addr, size_t heap_size) {
    // TODO: works?
    kmem_manager_bitmap_ptr = bitmap;

    g_kmalloc_data.heap_addr = heap_addr + PAGE_SIZE;
    g_kmalloc_data.allocated_chunks = 0;
    g_kmalloc_data.total_chunks = get_num_chunks(heap_size - PAGE_SIZE);

}

// TODO: manage g_kmalloc_data with these functions and use this to (de)allocate the 
// correct amount of space
void* kmalloc(size_t len) {
    
    size_t real_size = sizeof(kmalloc_header_t) + len;
    size_t needed_chunks = (real_size + CHUNK_SIZE - 1) / CHUNK_SIZE;

    if (needed_chunks > g_kmalloc_data.total_chunks - g_kmalloc_data.allocated_chunks){
        return NULL;
    }

    // TODO: see if this is compatible with chunks (prob not lol)
    size_t chunk_addr = bm_allocate(kmem_manager_bitmap_ptr, real_size);

    if (chunk_addr == 0) {
        println("yikes");
        return NULL;
    }

    kmalloc_header_t* header = (kmalloc_header_t*)(g_kmalloc_data.heap_addr + (chunk_addr * CHUNK_SIZE));
    uint8_t* ptr = header->data;
    header->size_in_chunks = needed_chunks;

    g_kmalloc_data.allocated_chunks += needed_chunks;

    println("kmalloc finished");
    return ptr;
}

void* kfree (void* addr, size_t len) {
    return NULL;
}

static size_t get_num_chunks (size_t mem_size) {
    return (sizeof(uint8_t) * mem_size) / (sizeof(uint8_t) * CHUNK_SIZE + 1);
}

