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

struct sec_cache_entry;

typedef struct {
  uint32_t blocksize;
  uint32_t cache_count;

  struct sec_cache_entry* entries[];
} fat_sector_cache_t;

void init_fat_cache(void);

ErrorOrPtr create_fat_info(vnode_t* node);
void destroy_fat_info(vnode_t* node);

fat_sector_cache_t* create_fat_sector_cache(uintptr_t block_size, uint32_t cache_count);
void destroy_fat_sector_cache(fat_sector_cache_t* cache);

int fatfs_read(vnode_t* node, void* buffer, size_t size, disk_offset_t offset);

struct fat_file* allocate_fat_file(struct file* file);
void deallocate_fat_file(struct fat_file* file);

#endif // !__ANIVA_FAT_CACHE__
