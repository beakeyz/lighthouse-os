#include "cache.h"
#include "dev/debug/serial.h"
#include "fs/fat/core.h"
#include "fs/vnode.h"
#include "libk/flow/error.h"
#include <mem/zalloc.h>

#define MAX_FAT_INFO_ENTRIES (28)

static zone_allocator_t __fat_info_cache;
static zone_allocator_t __fat_file_cache;

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

  zfree_fixed(&__fat_info_cache, info);

  node->fs_data.m_fs_specific_info = nullptr;
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

  result = init_zone_allocator(&__fat_info_cache, MAX_FAT_INFO_ENTRIES * sizeof(fat_fs_info_t), sizeof(fat_fs_info_t), NULL);

  ASSERT_MSG(!IsError(result), "Could not create FAT info cache!");

  result = init_zone_allocator(&__fat_file_cache, ZALLOC_DEFAULT_ALLOC_COUNT * sizeof(fat_file_t), sizeof(fat_file_t), NULL);

  ASSERT_MSG(!IsError(result), "Could not create FAT file cache!");
}
