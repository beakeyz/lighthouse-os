#include "cache.h"
#include "dev/debug/serial.h"
#include "dev/disk/generic.h"
#include "fs/fat/core.h"
#include "fs/file.h"
#include "fs/vnode.h"
#include "libk/flow/error.h"
#include "libk/string.h"
#include "logging/log.h"
#include "mem/heap.h"
#include "mem/kmem_manager.h"
#include <mem/zalloc.h>

#define DEFAULT_FAT_INFO_ENTRIES (8)
#define DEFAULT_FAT_SECTOR_CACHES (8)

static zone_allocator_t __fat_info_cache;
static zone_allocator_t __fat_file_cache;
static zone_allocator_t __fat_sec_cache_alloc;

ErrorOrPtr create_fat_info(vnode_t* node)
{
  fat_fs_info_t* info;

  if (!node)
    return Error();

  info = zalloc_fixed(&__fat_info_cache);

  if (!info)
    return Error();

  node->fs_data.m_fs_specific_info = info;

  return Success(0);
}

void destroy_fat_info(vnode_t* node)
{
  fat_fs_info_t* info;

  if (!node || !FAT_FSINFO(node))
    return;

  info = FAT_FSINFO(node);

  if (info->sector_cache)
    destroy_fat_sector_cache(info->sector_cache);

  zfree_fixed(&__fat_info_cache, info);

  node->fs_data.m_fs_specific_info = nullptr;
}

fat_sector_cache_t* create_fat_sector_cache(uintptr_t block_size)
{
  fat_sector_cache_t* cache;

  if (!block_size)
    return nullptr;

  cache = zalloc_fixed(&__fat_sec_cache_alloc);

  if (!cache)
    return nullptr;

  cache->blocksize = block_size;
  cache->fat_block_buffer = Must(__kmem_kernel_alloc_range(block_size, NULL, KMEM_FLAG_KERNEL | KMEM_FLAG_WRITABLE));

  return cache;
}

void destroy_fat_sector_cache(fat_sector_cache_t* cache)
{
  if (!cache)
    return;

  __kmem_kernel_dealloc(cache->fat_block_buffer, cache->blocksize);
  zfree_fixed(&__fat_sec_cache_alloc, cache);
}

/*!
 * @brief: Read a block into the fat block buffer
 *
 * TODO: make use of more advanced caching
 * (Or put caching in the generic disk core)
 */
static int 
__read(vnode_t* node, fat_sector_cache_t* cache, uintptr_t block)
{
  return pd_bread(node->m_device, (void*)cache->fat_block_buffer, block);
}

/*!
 * @brief Read a bit of data from fat
 *
 * Nothing to add here...
 */
int fat_read(vnode_t* node, void* buffer, size_t size, disk_offset_t offset)
{
  int error;
  fat_fs_info_t* info;
  uint64_t current_block, current_offset, current_delta;
  uint64_t lba_size, read_size;

  current_offset = 0;
  info = VN_FS_DATA(node).m_fs_specific_info;
  lba_size = VN_FS_DATA(node).m_blocksize;

  while (current_offset < size) {
    current_block = (offset + current_offset) / lba_size;
    current_delta = (offset + current_offset) % lba_size;

    error = __read(node, info->sector_cache, current_block);

    if (error)
      return -1;

    read_size = size - current_offset;

    /* Constantly shave off lba_size */
    if (read_size > lba_size - current_delta)
      read_size = lba_size - current_delta;

    memcpy(buffer + current_offset, &((uint8_t*)info->sector_cache->fat_block_buffer)[current_delta], read_size);

    current_offset += read_size;
  }

  return 0;
}

int fat_write(vnode_t* node, void* buffer, size_t size, disk_offset_t offset)
{
  kernel_panic("TODO: fat_write");
}

int fat_bread(vnode_t* node, void* buffer, size_t count, uint64_t sec)
{
  kernel_panic("TODO: fat_bread");
}

int fat_bwrite(vnode_t* node, void* buffer, size_t count, uint64_t sec)
{
  kernel_panic("TODO: fat_bwrite");
}

fat_file_t* create_fat_file(file_t* file)
{
  fat_file_t* ret;

  ret = zalloc_fixed(&__fat_file_cache);

  if (!ret)
    return nullptr;

  memset(ret, 0, sizeof(*ret));

  ret->parent = file;

  return ret;
}

void destroy_fat_file(fat_file_t* file)
{
  if (file->clusterchain_buffer)
    kfree(file->clusterchain_buffer);

  zfree_fixed(&__fat_file_cache, file);
}

/*
 * TODO: we have made a nice and fast FAT implementation in light loader, so we know
 * how fat is supposed to work a bit now. Reimplement (or actually just implement) a 
 * propper caching system here to make file lookups (clusterchain reads n shit) nice 
 * and fast plz =)
 */
void init_fat_cache(void)
{
  ErrorOrPtr result;

  result = init_zone_allocator(&__fat_info_cache, DEFAULT_FAT_INFO_ENTRIES * sizeof(fat_fs_info_t), sizeof(fat_fs_info_t), NULL);

  ASSERT_MSG(!IsError(result), "Could not create FAT info cache!");

  result = init_zone_allocator(&__fat_file_cache, ZALLOC_DEFAULT_ALLOC_COUNT * sizeof(fat_file_t), sizeof(fat_file_t), NULL);

  ASSERT_MSG(!IsError(result), "Could not create FAT file cache!");

  ASSERT_MSG(
      !IsError(init_zone_allocator(&__fat_sec_cache_alloc, DEFAULT_FAT_SECTOR_CACHES * sizeof(fat_sector_cache_t), sizeof(fat_sector_cache_t), NULL)),
      "Failed to create a FAT sector cache allocator!"
      );
}
