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
  uintptr_t fat_block_buffer;
  uintptr_t blocksize;
} fat_sector_cache_t;
void init_fat_cache(void);

ErrorOrPtr create_fat_info(vnode_t* node);
void destroy_fat_info(vnode_t* node);

fat_sector_cache_t* create_fat_sector_cache(uintptr_t block_size);
void destroy_fat_sector_cache(fat_sector_cache_t* cache);

int fat_read(vnode_t* node, void* buffer, size_t size, disk_offset_t offset);

struct fat_file* create_fat_file(struct file* file);
void destroy_fat_file(struct fat_file* file);

#endif // !__ANIVA_FAT_CACHE__
