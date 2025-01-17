#ifndef __ANIVA_FAT_CACHE__
#define __ANIVA_FAT_CACHE__

/*
 * This is the FAT cache housekeeping file for aniva
 */

#include "dev/disk/shared.h"
#include "dev/disk/volume.h"
#include "fs/core.h"
#include "libk/flow/error.h"

#define DEFAULT_SEC_CACHE_COUNT (8)
#define MAX_SEC_CACHE_COUNT (8)

struct fat_file;
struct file;
enum FAT_FILE_TYPE;

struct sec_cache_entry;

typedef struct {
    uint32_t blocksize;
    uint32_t cache_count;
    uint8_t* block_buffers;

    struct sec_cache_entry* entries[];
} fat_sector_cache_t;

void init_fat_cache(void);

kerror_t create_fat_info(fs_root_object_t* fsroot, volume_t* device);
void destroy_fat_info(fs_root_object_t* fsroot);

fat_sector_cache_t* create_fat_sector_cache(uintptr_t block_size, uint32_t cache_count);
void destroy_fat_sector_cache(fat_sector_cache_t* cache);

int fatfs_read(fs_root_object_t* fsroot, void* buffer, size_t size, disk_offset_t offset);
int fatfs_write(fs_root_object_t* fsroot, void* buffer, size_t size, disk_offset_t offset);
int fatfs_flush(fs_root_object_t* fsroot);

struct fat_file* allocate_fat_file(void* parent, enum FAT_FILE_TYPE type);
void deallocate_fat_file(struct fat_file* file);

#endif // !__ANIVA_FAT_CACHE__
