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

fat_sector_cache_t* create_fat_sector_cache(uint64_t block_size)
{
  fat_sector_cache_t* ret;

  ret = zalloc_fixed(&__fat_sec_cache_alloc);

  if (!ret)
    return nullptr;

  memset(ret, 0, sizeof(*ret));

  ret->sector_size = block_size;
  ret->buffer_allocator = create_zone_allocator(MAX_SEC_CACHE_COUNT * block_size, block_size, NULL);

  return ret;
}

void destroy_fat_sector_cache(fat_sector_cache_t* cache)
{
  void* current_buffer;

  for (uint64_t i = 0; i < MAX_SEC_CACHE_COUNT; i++) {
    current_buffer = cache->buffers[i];

    if (!current_buffer)
      continue;

    zfree_fixed(cache->buffer_allocator, current_buffer);
  }

  destroy_zone_allocator(cache->buffer_allocator, false);
  zfree_fixed(&__fat_sec_cache_alloc, cache);
}

/*!
 * @brief Get the buffer index for a certain offset into the fat filesystem
 *
 * Nothing to add here...
 */
static uint8_t fat_sec_cache_get_sector_index(fat_sector_cache_t* cache, uint64_t block)
{
  uint8_t lower_count = 0;
  uint8_t preferred_cache_idx = 0;
  uint8_t lowest_usecount = 0xFF;

  for (uint8_t i = 0; i < MAX_SEC_CACHE_COUNT; i++) {

    /* Free cache? Mark it */
    if (cache->sector_useage_counts[i] == 0) {
      preferred_cache_idx = i;
      continue;
    }

    /* If there already is a cache with this block, get that one */
    if (cache->sector_blocks[i] == block) {
      return i;
    }

    if (cache->sector_useage_counts[i] < lowest_usecount && !preferred_cache_idx) {
      lowest_usecount = cache->sector_useage_counts[i];
      lower_count++;
      preferred_cache_idx = i;
    }
  }

  /* If there was a cache with a lowest count, use that */
  if (lower_count)
    return preferred_cache_idx;

  preferred_cache_idx = cache->oldest_sector_idx;

  cache->oldest_sector_idx = (cache->oldest_sector_idx + 1) % 7;

  return preferred_cache_idx;
}

/*!
 * @brief Make sure the cache at an index @index is backed by memory
 *
 * Nothing to add here...
 */
static void checkout_fat_sector_cache(fat_sector_cache_t* cache, uint8_t index)
{
  void* buffer = cache->buffers[index];

  if (!buffer)
    cache->buffers[index] = zalloc_fixed(cache->buffer_allocator);
  else
    memset(buffer, 0, cache->sector_size);

  cache->sector_useage_counts[index] = NULL;
  cache->sector_blocks[index] = NULL;
}

/*!
 * @brief Gets the cache where we put the block, or puts the block in a new cache
 *
 * Nothing to add here...
 */
static uint8_t 
__cached_read(vnode_t* node, fat_sector_cache_t* cache, uintptr_t block)
{
  int error;
  uint64_t disk_blocks;
  uint8_t cache_idx = fat_sec_cache_get_sector_index(cache, block);

  /* Block already in our cache */
  if (cache->sector_blocks[cache_idx] == block)
    goto success;

  /* Ensure we have a buffer here */
  checkout_fat_sector_cache(cache, cache_idx);

  /* How many disk blocks do we need to read until we have read one FAT block? (sector) */
  disk_blocks = ALIGN_UP(cache->sector_size, node->m_device->m_block_size) / node->m_device->m_block_size;

  for (uint64_t i = 0; i < disk_blocks; i++) {

    /* Read the thing */
    error = pd_bread(node->m_device, cache->buffers[cache_idx] + (i * node->m_device->m_block_size), block + i);

    if (error)
      goto out;
  }

success:
  cache->sector_blocks[cache_idx] = block;
  cache->sector_useage_counts[cache_idx]++;

  return cache_idx;

out:
  return 0xFF;
}

/*!
 * @brief Read a 
 *
 * Nothing to add here...
 */
int fat_read(vnode_t* node, void* buffer, size_t size, disk_offset_t offset)
{
  fat_fs_info_t* info;
  uint64_t current_block, current_offset, current_delta;
  uint64_t lba_size, read_size;
  uint8_t current_cache_idx;

  current_offset = 0;
  info = VN_FS_DATA(node).m_fs_specific_info;
  lba_size = info->sector_cache->sector_size;

  while (current_offset < size) {
    current_block = (offset + current_offset) / lba_size;
    current_delta = (offset + current_offset) % lba_size;

    current_cache_idx = __cached_read(node, info->sector_cache, current_block);

    if (current_cache_idx == 0xFF)
      return -1;

    read_size = size - current_offset;

    /* Constantly shave off lba_size */
    if (read_size > lba_size - current_delta)
      read_size = lba_size - current_delta;

    memcpy(buffer + current_offset, &(info->sector_cache->buffers[current_cache_idx])[current_delta], read_size);

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
  uint8_t index;
  uintptr_t offset;
  void* cache_buffer;
  fat_fs_info_t* info;

  if (!node || !buffer)
    return -1;

  if (!count)
    return -2;

  info = VN_FS_DATA(node).m_fs_specific_info;

  for (uintptr_t i = 0; i < count; i++) {
    offset = (i * info->sector_cache->sector_size);
    index = __cached_read(node, info->sector_cache, sec + offset);

    if (!info->sector_cache->sector_useage_counts[index])
      return -3;

    cache_buffer = info->sector_cache->buffers[index];

    memcpy(buffer + offset, cache_buffer, info->sector_cache->sector_size);
  }

  return 0;
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
