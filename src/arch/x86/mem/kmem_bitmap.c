#include "kmem_bitmap.h"
#include "arch/x86/interupts/control/pic.h"
#include "arch/x86/kmain.h"
#include "arch/x86/mem/kmem_manager.h"
#include <libc/stddef.h>
#include <arch/x86/dev/debug/serial.h>
#include <libc/string.h>

struct scan_attempt {
    size_t indx;
    size_t len;
};

static int find_trailing_zeroes (size_t value);

void init_bitmap(kmem_bitmap_t *map, struct multiboot_tag_basic_meminfo *basic_info, uint32_t addr) {
    
}

static int find_trailing_zeroes (size_t value) {
    for (size_t i = 0; i < 8 * sizeof(size_t); ++i) {
        if ((value >> i) & 1) {
            return i;
        }
    }
    return 8 * sizeof(size_t);
}

size_t bm_find(kmem_bitmap_t* map, size_t len) {

    const size_t bit_size = 8 * sizeof(size_t);
    uint64_t* data = (uint64_t*)map->bm_memory_map;

    uintptr_t start_byte = 0;
    uintptr_t start_bit = 0;

    uintptr_t free_chunk_counter = 0;

    println("trying to find chunks");

    for (uintptr_t i = start_byte; i < map->bm_size; i++) {
        if (data[i] == BITMAP_ENTRY_FULL) {
            free_chunk_counter = 0;
            start_bit = 0;
            println("skipped full one");
            continue;
        }
        
        if (data[i] == 0x0) {
            free_chunk_counter += bit_size;
            start_bit = 0;
            
            if (free_chunk_counter >= len) {
                println("yay, found cool chunk");
                return free_chunk_counter;
            }

            println("skipped empty one");
            continue;
        }

        uintptr_t current = data[i];
        uint8_t skipped_bits = start_bit;

        current >>= skipped_bits;
        uint32_t trailing_zeroes = 0;

        println("scanning bitline");
        while (skipped_bits < bit_size) {
            if (current == 0) {
                if (free_chunk_counter == 0) {
                    // WHAHA
                }
                free_chunk_counter += bit_size - skipped_bits;
                skipped_bits = bit_size;
                println("start found lol");
            } else {
                trailing_zeroes = find_trailing_zeroes(current);
                current >>= trailing_zeroes;

                free_chunk_counter += trailing_zeroes;
                skipped_bits += trailing_zeroes;

                if (free_chunk_counter == len) {
                    println("something");
                    return free_chunk_counter;
                }

                uint32_t trailing_ones = find_trailing_zeroes(~current);
                current >>= trailing_ones;
                skipped_bits+= trailing_ones;
                free_chunk_counter = 0;
                println("looped around");
            }
        }
    }

    if (free_chunk_counter < len) {
        print("thats a yikes! could not find free chunks D=");
        asm volatile ("hlt");
    }

    return free_chunk_counter;
}

size_t bm_allocate(kmem_bitmap_t* map, size_t len) {
    size_t block = bm_find(map, len);
    if (block == 0) {
        println("failed to find free space in bitmap");
        return 0;
    }

    // bm_mark_block_used is going to scream when it fails anyway
    if (bm_mark_block_used(map, block, len) != 0) {
        println("returned the thing =D");
        return block;
    }
    return 0;
}

size_t bm_mark_block_used(kmem_bitmap_t* map, size_t idx, size_t len) {
    uint8_t* start = &map->bm_memory_map[idx / 8];
    uint8_t* end = &map->bm_memory_map[(idx + len) / 8];

    if (start == end) {
        memset(start, 0xff, 1);
    } else {
        memset(start, 0xff, end - start);
    }
    return 1;
}

size_t bm_mark_block_free(kmem_bitmap_t* map, size_t idx, size_t len) {
   
    return NULL;
}


bool bm_is_in_range(kmem_bitmap_t* map, size_t idx) {
    return idx < map->bm_size;
}

uint32_t find_kernel_entries(uint64_t addr){
    uint32_t kernel_entries = ((uint64_t)addr) / PAGE_SIZE_BYTES;
    uint32_t kernel_mod_entries = ((uint32_t)(addr)) % PAGE_SIZE_BYTES;
    if (  kernel_mod_entries != 0){
        return kernel_entries + 2;
    } 
    return kernel_entries + 1;
   
}

void bm_get_region(kmem_bitmap_t* map, uint64_t* base_address, size_t* length_in_bytes)
{
    *base_address = (uint64_t)map->bm_memory_map;
    *length_in_bytes = map->bm_size / 8 + 1;
}
