#include "core.h"
#include "libk/flow/error.h"
#include "sync/mutex.h"

/*
 * Translate a fs-local index to a block number and offset within that block 
 * on the actual partitioned device
 */
static int ffile_block_info(fat_file_t* file, int index, int* offset, uintptr_t* blocknr)
{
  kernel_panic("TODO: fatfile_block_info");
}

/*
 * Get the current cluster that this fat_file is pointing to
 */
static int ffile_get(fat_file_t* file)
{
  kernel_panic("TODO: fatfile_get");
}

/*
 * Put a cluster into the clusterchain
 */
static int ffile_put(fat_file_t* file, int cluster)
{
  kernel_panic("TODO: fatfile_put");
}

/*
 * Grab the next cluster in the chain
 */
static int ffile_next_cluster(fat_file_t* file)
{
  kernel_panic("TODO: fatfile_next_cluster");
}

/*
 * Set the index of the file inside the clusterchain
 */
static int ffile_seek_index(fat_file_t* file, int index)
{
  kernel_panic("TODO: fatfile_seek_index");
}

/* Very linux-y */
int fat_prepare_finfo(struct fs_superblock* sb)
{
  fat_fs_info_t* info = FAT_FSINFO(sb);

  info->fat_lock = create_mutex(0);

  if (is_fat32(info)) {
    info->fat_file_shift = 2;
    return 0;
  }

  if (is_fat16(info)) {
    info->fat_file_shift = 1;
    return 0;
  }

  if (is_fat12(info)) {
    info->fat_file_shift = -1;
    return 0;
  }

  return -1;
}
