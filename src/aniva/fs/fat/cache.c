#include "cache.h"
#include "fs/fat/core.h"
#include "fs/vnode.h"
#include "libk/flow/error.h"
#include <mem/zalloc.h>

#define MAX_FAT_INFO_ENTRIES (28)

static zone_allocator_t* __fat_info_cache;
static zone_allocator_t* __fat_file_cache;

ErrorOrPtr create_fat_info(vnode_t* node)
{
  fat_fs_info_t* info;

  if (!node)
    return Error();

  info = zalloc_fixed(__fat_info_cache);

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

  zfree_fixed(__fat_info_cache, info);

  node->fs_data.m_fs_specific_info = nullptr;
}

void init_fat_cache(void)
{
  __fat_info_cache = create_zone_allocator_ex(nullptr, NULL, MAX_FAT_INFO_ENTRIES * sizeof(fat_fs_info_t), sizeof(fat_fs_info_t), NULL);
  __fat_file_cache = create_zone_allocator_ex(nullptr, NULL, 2 * Mib, sizeof(fat_file_t), NULL);

  ASSERT_MSG(__fat_file_cache, "Could not create FAT file cache!");
  ASSERT_MSG(__fat_info_cache, "Could not create FAT info cache!");
}
