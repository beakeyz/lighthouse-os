#ifndef __ANIVA_DISK_DEV_BLK_CACHE__
#define __ANIVA_DISK_DEV_BLK_CACHE__

#include <libk/stddef.h>

struct partitioned_disk_dev;

#define DISK_CACHE_FLAG_USED    0x00000001
#define DISK_CACHE_FLAG_DIRTY   0x00000002

typedef struct disk_blk_cache_entry {
  uint32_t flags;
  uint32_t usage_count;
  uint64_t blk_idx;
  uint8_t data[];
} disk_blk_cache_entry_t;

#define DISK_DEFAULT_CACHE_CAPACITY 64

typedef struct disk_blk_cache {
  size_t total_size;
  uint32_t blk_size;
  uint32_t blk_capacity;

  struct partitioned_disk_dev* dev;

  disk_blk_cache_entry_t blk_entries[];
} disk_blk_cache_t;

disk_blk_cache_t* create_disk_blk_cache(struct partitioned_disk_dev* dev, uint32_t capacity, uint32_t blocksize);
void destroy_disk_blk_cache(disk_blk_cache_t* cache);

int disk_cached_read(disk_blk_cache_t* cache, void* buffer, size_t size, uintptr_t offset);
int disk_cached_write(disk_blk_cache_t* cache, void* buffer, size_t size, uintptr_t offset);

int disk_cache_flush(disk_blk_cache_t* cache);

#endif // !__ANIVA_DISK_DEV_BLK_CACHE__
