#ifndef __ANIVA_FAT_CACHE__
#define __ANIVA_FAT_CACHE__

/*
 * This is the FAT cache housekeeping file for aniva
 */

#include "fs/vnode.h"
#include "libk/flow/error.h"
#include "mem/zalloc.h"

#define DEFAULT_SEC_CACHE_COUNT  (8)
#define MAX_SEC_CACHE_COUNT  (64)

struct fat_file;
struct file;

typedef struct {
  /* These bits indicate wether a certain buffer may be flushed */
  uint64_t dirty_bits;

  uint32_t sector_size;
  uint8_t oldest_sector_idx;

  /* Allocator for our buffers */
  zone_allocator_t* buffer_allocator;

  uint16_t sector_useage_counts[MAX_SEC_CACHE_COUNT];
  uint64_t sector_blocks[MAX_SEC_CACHE_COUNT];
  uint8_t* buffers[MAX_SEC_CACHE_COUNT];
} fat_sector_cache_t;

void init_fat_cache(void);

ErrorOrPtr create_fat_info(vnode_t* node);
void destroy_fat_info(vnode_t* node);

fat_sector_cache_t* create_fat_sector_cache(uint64_t block_size);
void destroy_fat_sector_cache(fat_sector_cache_t* cache);

int fat_read(vnode_t* node, void* buffer, size_t size, disk_offset_t offset);
int fat_write(vnode_t* node, void* buffer, size_t size, disk_offset_t offset);

/* Sector read functions */
int fat_bread(vnode_t* node, void* buffer, size_t size, uint64_t sec);
int fat_bwrite(vnode_t* node, void* buffer, size_t size, uint64_t sec);

struct fat_file* create_fat_file(struct file* file);
void destroy_fat_file(struct fat_file* file);

#endif // !__ANIVA_FAT_CACHE__
