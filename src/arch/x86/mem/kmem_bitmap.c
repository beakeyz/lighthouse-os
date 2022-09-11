#include "kmem_bitmap.h"
#include "arch/x86/kmain.h"
#include "arch/x86/mem/kmem_manager.h"
#include <libc/stddef.h>
#include <arch/x86/dev/debug/serial.h>
#include <libc/string.h>

struct scan_attempt {
    size_t indx;
    size_t len;
};

void init_bitmap(kmem_bitmap_t *map, struct multiboot_tag_basic_meminfo *basic_info, uint32_t addr) {
    map->bm_memory_map = (uint64_t*) &_kernel_end;
    size_t mem_size = (basic_info->mem_upper + 1024) * 1024;
    map->bm_size = mem_size / PAGE_SIZE + 1;
    map->bm_used_frames = 0;
    map->bm_entry_num = map->bm_size / 64 + 1;

    map->bm_memory_map = get_bitmap_region(addr, map->bm_size / 8 + 1);

    // FIXME: this crashes after a bunch of loops, why
    for (uint32_t i = 0; i < map->bm_entry_num; i++) {
        map->bm_memory_map[i] = 0x0;
    }

    uint32_t kernel_entries = find_kernel_entries(addr);
    uint32_t rows_num = kernel_entries / 64;
    
    uint32_t j;
    for (j = 0; j < rows_num; j++) {
        map->bm_memory_map[j] = ~(0);
    }
    map->bm_memory_map[j] = ~(~(0ul) << (kernel_entries - (rows_num * 64)));
    map->bm_used_frames = kernel_entries;

}

bool bm_get(kmem_bitmap_t* map, size_t idx) {
    if (!bm_is_in_range(map, idx)) {
        println("bitmap access was out of range!");
        return false;
    }
    // get the correct bit from the index
    size_t bm_bit = idx % 8;
    // get the byte in which the correct bit resides
    size_t bm_byte = idx / 8;
    // return the bit we want from the byte its in
    return map->bm_memory_map[bm_byte] & (1 << bm_bit);
}

void bm_set(kmem_bitmap_t* map, size_t idx, bool value) {
    if (!bm_is_in_range(map, idx)) {
        println("bitmap set was out of range!");
        return;
    }
    // get the correct bit from the index
    size_t bm_bit = idx % 8;
    // get the byte in which the correct bit resides
    size_t bm_byte = idx / 8;
    
    if (value) {
        map->bm_memory_map[bm_byte] |= (1 << bm_bit);
        return;
    }
    map->bm_memory_map[bm_byte] &= ~(1 << bm_bit);
    return;
}

size_t bm_find(kmem_bitmap_t* map, size_t len) {
    // im paranoid
    if (map->bm_last_allocated_bit > map->bm_size) {
        map->bm_last_allocated_bit = 0;
        println("reset the allocation pointer (out of bounds (idk whtf happend but sure))");
    }

    //static int recursive_lock = 0;
    struct scan_attempt attempt = {0,0};

    for (int i = map->bm_last_allocated_bit; i < map->bm_size; i++) {
        // skip the first entry
        if (i == 0)
            continue;

        // we need to find a contingues batch of bits
        
        // if the bit is free
        if (!bm_get(map, i)) {
            // if we found a free bit for the first time
            if (attempt.len == 0) {
                // reset index
                attempt.indx = i;
            } 
            attempt.indx++;
        } else {
            // reset
            // TODO: see if we need to store the data of a failed find,
            // because we could potentially start the next search at the end of 
            // the previous search 
            attempt.indx = attempt.len = 0;
        }

        // compare
        if (attempt.len == len) {
            map->bm_last_allocated_bit = attempt.indx + len;
            return attempt.indx;
        }
    }

    // nothing found
    if (map->bm_last_allocated_bit == 0 || attempt.indx == 0 || attempt.len == 0) {
        println("no bitmap entry found! This is bad");
        return 0;
    }

    // attempt a recursive search
    map->bm_last_allocated_bit = 0;
    // Check is redundant, because we might not find anything for a while
    // When there is some free space at the end of the map
    /*
    recursive_lock++;
    if (recursive_lock > MAX_FIND_ATTEMPTS) {
        recursive_lock = 0;
        println("nothing came up after MAX_FIND_ATTEMPTS amount of tries =/");
        return 0;
    } */
    return bm_find(map, len);
}

size_t bm_allocate(kmem_bitmap_t* map, size_t len) {
    size_t block = bm_find(map, len);
    if (block == 0) {
        println("failed to find free space in bitmap");
        return 0;
    }

    // bm_mark_block_used is going to scream when it fails anyway
    if (bm_mark_block_used(map, block, len) != 0) {
        return block;
    }
    return 0;
}

size_t bm_mark_block_used(kmem_bitmap_t* map, size_t idx, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (bm_get(map, idx + i) == true) {
            // TODO: perhaps reset the set entries?
            println("marking as used while already in use!");
            return 0;
        }
        bm_set(map, idx + i, true);
    }
    // success
    return 1;
}

size_t bm_mark_block_free(kmem_bitmap_t* map, size_t idx, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (bm_get(map, idx + i) == false) {
            // TODO: perhaps reset the set entries?
            println("marking as free while already free!");
            return 0;
        }
        bm_set(map, idx + i, true);
    }
    // success
    return 1;
}


bool bm_is_in_range(kmem_bitmap_t* map, size_t idx) {
    return idx < map->bm_size;
}

uint32_t find_kernel_entries(uint64_t addr){
    uint32_t kernel_entries = ((uint64_t)addr) / PAGE_SIZE;
    uint32_t kernel_mod_entries = ((uint32_t)(addr)) % PAGE_SIZE;
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
