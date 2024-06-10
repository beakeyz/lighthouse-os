#ifndef __ANIVA_FAT_CACHE__
#define __ANIVA_FAT_CACHE__

/*
 * This is the FAT cache housekeeping file for aniva
 */

#include "dev/disk/generic.h"
#include "dev/disk/shared.h"
#include "libk/flow/error.h"
#include "oss/node.h"

#define DEFAULT_SEC_CACHE_COUNT (8)
#define MAX_SEC_CACHE_COUNT (8)

struct fat_file;
struct file;
enum FAT_FILE_TYPE;

struct sec_cache_entry;

typedef struct {
    uint32_t blocksize;
    uint32_t cache_count;

    struct sec_cache_entry* entries[];
} fat_sector_cache_t;

void init_fat_cache(void);

ErrorOrPtr create_fat_info(oss_node_t* node, partitioned_disk_dev_t* device);
void destroy_fat_info(oss_node_t* node);

fat_sector_cache_t* create_fat_sector_cache(uintptr_t block_size, uint32_t cache_count);
void destroy_fat_sector_cache(fat_sector_cache_t* cache);

int fatfs_read(oss_node_t* node, void* buffer, size_t size, disk_offset_t offset);
int fatfs_write(oss_node_t* node, void* buffer, size_t size, disk_offset_t offset);
int fatfs_flush(oss_node_t* node);

struct fat_file* allocate_fat_file(void* parent, enum FAT_FILE_TYPE type);
void deallocate_fat_file(struct fat_file* file);

#endif // !__ANIVA_FAT_CACHE__
