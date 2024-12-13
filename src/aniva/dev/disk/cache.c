#include "cache.h"
#include "libk/flow/error.h"
#include "mem/kmem_manager.h"

static inline int disk_cache_get_entry(disk_blk_cache_t* cache, disk_blk_cache_entry_t** entry, uint32_t idx)
{
    if (!entry)
        return -1;

    *entry = (disk_blk_cache_entry_t*)(((uintptr_t)cache->blk_entries) + (cache->blk_size + sizeof(disk_blk_cache_entry_t)) * idx);
    return 0;
}

static int disk_cache_find_least_used_entry(disk_blk_cache_t* cache, disk_blk_cache_entry_t** entry)
{
    disk_blk_cache_entry_t* least_used;
    disk_blk_cache_entry_t* current;

    if (!entry)
        return -1;

    if (disk_cache_get_entry(cache, &least_used, 0))
        return -1;

    for (uint32_t i = 1; i < cache->blk_capacity; i++) {
        if (disk_cache_get_entry(cache, &current, i))
            return -2;

        if (current->usage_count < least_used->usage_count)
            least_used = current;
    }

    *entry = least_used;
    return 0;
}

/*!
 * @brief: Find a cache entry for a specific block, or a free one if we didn't already have this block cached
 */
static int disk_cache_find(disk_blk_cache_t* cache, disk_blk_cache_entry_t** entry, uintptr_t block_idx)
{
    int error;
    disk_blk_cache_entry_t* free;
    disk_blk_cache_entry_t* current;

    free = nullptr;
    current = nullptr;

    for (uintptr_t i = 0; i < cache->blk_capacity; i++) {
        error = disk_cache_get_entry(cache, &current, i);

        if (error)
            return error;

        /* Found the block we wanted */
        if (current->blk_idx == block_idx) {
            current->usage_count++;
            *entry = current;
            break;
        }

        /* Found a free entry */
        if ((current->flags & DISK_CACHE_FLAG_USED) != DISK_CACHE_FLAG_USED && !free) {
            free = current;
            break;
        }

        error = -2;
    }

    if (error)
        disk_cache_find_least_used_entry(cache, &free);

    ASSERT_MSG(free, "[Disk cache] could not find a least used cache entry for some reason?");

    free->usage_count++;
    *entry = free;
    return 0;
}

static inline void disk_cache_clear_entry(disk_blk_cache_t* cache, disk_blk_cache_entry_t* entry)
{
    memset(entry, 0, sizeof(*entry));
}

/*!
 * @brief: Allocate memory for a disk cache
 */
disk_blk_cache_t* create_disk_blk_cache(struct partitioned_disk_dev* dev, uint32_t capacity, uint32_t blocksize)
{
    size_t needed_size;
    disk_blk_cache_t* cache;

    if (!capacity)
        capacity = DISK_DEFAULT_CACHE_CAPACITY;

    /* Calculate how much memory we need */
    needed_size = ALIGN_UP(sizeof(*cache) + (capacity * (blocksize + sizeof(disk_blk_cache_entry_t))), SMALL_PAGE_SIZE);

    /* Allocate */
    ASSERT(!kmem_kernel_alloc_range((void**)&cache, needed_size, NULL, KMEM_FLAG_KERNEL | KMEM_FLAG_WRITABLE));

    if (!cache)
        return nullptr;

    /* Make sure the entire cache range is zero */
    memset(cache, 0, needed_size);

    cache->dev = dev;
    cache->blk_capacity = capacity;
    cache->blk_size = blocksize;
    cache->total_size = needed_size;

    return cache;
}

void destroy_disk_blk_cache(disk_blk_cache_t* cache)
{
    ASSERT(!kmem_kernel_dealloc((uintptr_t)cache, cache->total_size));
}

static int _disk_read_blk_into_cache(disk_blk_cache_t* cache, uintptr_t block_idx)
{
    int error;
    disk_blk_cache_entry_t* entry;

    error = disk_cache_find(cache, &entry, block_idx);

    if (error)
        return error;

    return 0;
}

int disk_cached_read(disk_blk_cache_t* cache, void* buffer, size_t size, uintptr_t offset)
{
    _disk_read_blk_into_cache(cache, 0);
    return 0;
}

int disk_cached_write(disk_blk_cache_t* cache, void* buffer, size_t size, uintptr_t offset);

int disk_cache_flush(disk_blk_cache_t* cache);
