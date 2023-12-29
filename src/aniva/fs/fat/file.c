#include "file.h"
#include "fs/fat/cache.h"
#include "fs/fat/core.h"
#include "fs/file.h"
#include "fs/vobj.h"
#include "libk/flow/error.h"
#include "logging/log.h"

int fat_read(file_t* file, void* buffer, size_t* size, uintptr_t offset)
{
  vnode_t* node = file->m_obj->m_parent;

  if (!size)
    return -1;

  if (offset >= file->m_total_size)
    return -2;

  /* Trim the size a bit if it happens to overflows */
  if ((offset + *size) >= file->m_total_size)
    *size -= ((offset + *size) - file->m_total_size);

  return fat32_load_clusters(node, buffer, file->m_private, offset, *size);
}

int fat_write(file_t* file, void* buffer, size_t* size, uintptr_t offset)
{
  kernel_panic("TODO: fat_write");
  return 0;
}

int fat_sync(file_t* file)
{
  kernel_panic("TODO: fat_sync");
  return 0;
}

int fat_close(file_t* file)
{
  fat_file_t* fat_file;

  fat_file = file->m_private;

  println(" - Closing FAT file!");
  destroy_fat_file(fat_file);
  return 0;
}

file_ops_t fat_file_ops = {
  .f_close = fat_close,
  .f_read = fat_read,
  .f_write = fat_write,
  .f_sync = fat_sync,
};

file_t* create_fat_file(fat_fs_info_t* info, uint32_t flags, char* path)
{
  file_t* ret;
  fat_file_t* ffile;

  ret = nullptr;
  ffile = nullptr;

  /* Invaid args */
  if (!info || !path || !path[0])
    return nullptr;

  ret = create_file(info->node, flags, path);

  if (!ret)
    return nullptr;

  /* This will make both the fat file and the regular file aware of each other */
  ffile = allocate_fat_file(ret);

  if (!ffile)
    goto fail_and_exit;

  file_set_ops(ret, &fat_file_ops);

  return ret;
fail_and_exit:
  destroy_vobj(ret->m_obj);
  return nullptr;
}

void destroy_fat_file(fat_file_t* file)
{
  if (file->clusterchain_buffer)
    kfree(file->clusterchain_buffer);

  deallocate_fat_file(file);
}

/*!
 * @brief: Compute the size of a FAT file
 *
 * We count the amount of clusters inside its clusterchain buffer
 * NOTE: we'll need to keep this buffer updated, meaning that we need to reallocate it
 * every time we write (and sync) this file
 */
size_t get_fat_file_size(fat_file_t* file)
{
  vnode_t* parent;
  
  if (!file || !file->parent || !file->parent->m_obj)
    return NULL;

  parent = file->parent->m_obj->m_parent;

  if (!parent)
    return NULL;

  uint32_t file_cluster_count = (file->clusters_num);
  return file_cluster_count * FAT_FS_INFO(parent)->cluster_size;
}
