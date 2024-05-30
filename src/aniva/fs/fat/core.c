#include "dev/core.h"
#include "dev/disk/generic.h"
#include "dev/driver.h"
#include "fs/core.h"
#include "fs/dir.h"
#include "fs/fat/cache.h"
#include "fs/fat/core.h"
#include "fs/fat/file.h"
#include "fs/file.h"
#include "libk/flow/error.h"
#include "libk/math/log2.h"
#include "logging/log.h"
#include "mem/heap.h"
#include "oss/core.h"
#include <libk/stddef.h>

/* Driver predefinitions */
int fat32_exit();
int fat32_init();

enum FAT_ERRCODES {
  FAT_OK = 0,
  FAT_INVAL = 1,
};

#define TO_UPPERCASE(c) ((c >= 'a' && c <= 'z') ? c - 0x20 : c)
#define TO_LOWERCASE(c) ((c >= 'A' && c <= 'Z') ? c + 0x20 : c)

typedef uintptr_t fat_offset_t;

/* TODO: */
//static int fat_parse_lfn(void* dir, fat_offset_t* offset);
//static int fat_parse_short_fn(fat_dir_entry_t* dir, const char* name);

static int parse_fat_bpb(fat_boot_sector_t* bpb, oss_node_t* block) 
{
  if (!fat_valid_bs(bpb)) {
    return FAT_INVAL;
  }

  if (!bpb->fats || !bpb->reserved_sectors) {
    return FAT_INVAL;
  }

  if (!POWER_OF_2(bpb->sector_size) || bpb->sector_size < 512 || bpb->sector_size > SMALL_PAGE_SIZE) {
    return FAT_INVAL;
  }

  if (!POWER_OF_2(bpb->sectors_per_cluster)) {
    return FAT_INVAL;
  }

  if (bpb->sectors_per_fat16 == 0 && bpb->fat32.sectors_per_fat == 0) {
    return FAT_INVAL;
  }

  /* TODO: fill data in superblock with useful data */
  oss_node_getfs(block)->m_blocksize = bpb->sector_size;

  return FAT_OK;
}

/*!
 * @brief Load a singular clusters value
 *
 * @node: the filesystem object to opperate on
 * @buffer: the buffer for the clusters value
 * @cluster: the 'index' of the cluster on disk
 */
static int
fat32_load_cluster(oss_node_t* node, uint32_t* buffer, uint32_t cluster)
{
  int error;
  fat_fs_info_t* info;

  info = GET_FAT_FSINFO(node);
  error = fatfs_read(node, buffer, sizeof(uint32_t), info->usable_sector_offset + (cluster * sizeof(uint32_t)));

  if (error)
    return error;

  /* Mask any fucky bits to comply with FAT32 standards */
  *buffer &= 0x0fffffff;

  return 0;
}

/*!
 * @brief: Set a singular clusters value
 *
 * @node: the filesystem object to opperate on
 * @buffer: the buffer for the clusters value
 * @cluster: the 'index' of the cluster on disk
 */
/* static */ int
fat32_set_cluster(oss_node_t* node, uint32_t buffer, uint32_t cluster)
{
  int error;
  fat_fs_info_t* info;

  /* Mask any fucky bits to comply with FAT32 standards */
  buffer &= 0x0fffffff;

  info = GET_FAT_FSINFO(node);
  error = fatfs_write(node, &buffer, sizeof(uint32_t), info->usable_sector_offset + (cluster * sizeof(uint32_t)));

  if (error)
    return error;

  return 0;
}

/*!
 * @brief: Caches the cluster chain for a file
 *
 * The file is responsible for managing the buffer after this point
 */
static int fat32_cache_cluster_chain(oss_node_t* node, fat_file_t* file, uint32_t start_cluster)
{
  int error;
  size_t length;
  uint32_t buffer;
  
  if (start_cluster < 0x2 || start_cluster > FAT_ENT_EOF)
    return -1;

  if (!file)
    return -2;

  length = 1;
  buffer = start_cluster;

  if (file->clusterchain_buffer)
    return -3;

  /* Count the clusters */
  while (true) {
    error = fat32_load_cluster(node, &buffer, buffer);

    if (error)
      return error;

    if (buffer < 0x2 || buffer >= FAT_ENT_EOF)
      break;

    length++;
  }

  /* Set correct lengths */
  file->clusters_num = length;

  if (!length)
    return 0;

  /* Reset buffer */
  buffer = start_cluster;

  /* Allocate chain */
  file->clusterchain_buffer = kmalloc(length * sizeof(uint32_t));

  /* Loop and store the clusters */
  for (uint32_t i = 0; i < length; i++) {
    file->clusterchain_buffer[i] = buffer;

    error = fat32_load_cluster(node, &buffer, buffer);

    if (error)
      return error;
  }
  
  return 0;
}

/*!
 * @brief: Write one entrie cluster to disk
 * 
 * The size of @buffer must be at least ->cluster_size 
 * this size is assumed
 */
static int
__fat32_write_cluster(oss_node_t* node, void* buffer, uint32_t cluster)
{
  int error;
  fs_oss_node_t* fsnode;
  fat_fs_info_t* info;
  uintptr_t current_cluster_offset;

  fsnode = oss_node_getfs(node);

  if (!fsnode || !fsnode->m_fs_priv || cluster < 2)
    return -1;

  info = fsnode->m_fs_priv;
  current_cluster_offset = (info->usable_clusters_start + (cluster - 2) * info->boot_sector_copy.sectors_per_cluster) * info->boot_sector_copy.sector_size;
  error = fatfs_write(node, buffer, info->cluster_size, current_cluster_offset);

  if (error)
    return -2;

  return 0;
}

int fat32_read_clusters(oss_node_t* node, uint8_t* buffer, fat_file_t* file, uint32_t start, size_t size)
{
  int error;
  uintptr_t current_index;
  uintptr_t current_offset;
  uintptr_t current_deviation;
  uintptr_t current_delta;
  uintptr_t current_cluster_offset;
  uintptr_t index;
  fat_fs_info_t* info;

  if (!node)
    return -1;

  info = GET_FAT_FSINFO(node);

  index = 0;

  while (index < size) {
    current_offset = start + index;
    current_index = current_offset / info->cluster_size;
    current_deviation = current_offset % info->cluster_size;

    current_delta = size - index;

    /* Limit the delta to the size of a cluster, except if we try to get an unaligned size smaller than 'cluster_size' */
    if (current_delta > info->cluster_size - current_deviation)
      current_delta = info->cluster_size - current_deviation;

    current_cluster_offset =
      (info->usable_clusters_start + (file->clusterchain_buffer[current_index] - 2)
      * info->boot_sector_copy.sectors_per_cluster) 
      * info->boot_sector_copy.sector_size;

    error = fatfs_read(node, buffer + index, current_delta, current_cluster_offset + current_deviation);

    if (error)
      return -2;

    index += current_delta;
  }

  return 0;
}

/*!
 * @brief: Finds the index of a free cluster
 *
 * @returns: 0 on success, anything else on failure (TODO: real ierror API)
 * @buffer: buffer where the index gets put into
 */
static int
__fat32_find_free_cluster(oss_node_t* node, uint32_t* buffer)
{
  int error;
  uint32_t current_index;
  uint32_t cluster_buff;
  fs_oss_node_t* fsnode;
  fat_fs_info_t* info;

  fsnode = oss_node_getfs(node);

  if (!fsnode)
    return -KERR_INVAL;

  info = fsnode->m_fs_priv;

  /* Start at index 2, cuz the first 2 clusters are weird lol */
  current_index = 2;
  cluster_buff = 0;

  while (current_index < info->cluster_count) {

    /* Try to load the value of the cluster at our current index into the value buffer */
    error = fat32_load_cluster(node, &cluster_buff, current_index);

    if (error)
      return error;

    /* Zero in a cluster index means its free / unused */
    if (cluster_buff == 0)
      break;

    current_index++;
  }

  /* Could not find a free cluster, WTF */
  if (current_index >= info->cluster_count)
    return -1;

  /* Place the index into the buffer */
  *buffer = current_index;

  return 0;
}

static uint32_t __fat32_dir_entry_get_start_cluster(fat_dir_entry_t* e)
{
  return ((uint32_t)e->first_cluster_low & 0xffff) | ((uint32_t)e->first_cluster_hi << 16);
}

/*!
 * @brief: Get the first cluster entry from a file
 */
static inline uint32_t __fat32_file_get_start_cluster_entry(fat_file_t* ff)
{
  if (!ff->clusterchain_buffer)
    return 0;

  return ff->clusterchain_buffer[0];
}

/*!
 * @brief: Find the last cluster of a fat directory entry
 *
 * @returns: 0 on success, anything else on failure
 */
static int 
__fat32_find_end_cluster(oss_node_t* node, uint32_t start_cluster, uint32_t* buffer)
{
  int error;
  uint32_t previous_value;
  uint32_t value_buffer;

  value_buffer = start_cluster;
  previous_value = value_buffer;

  while (true) {
    /* Load one cluster */
    error = fat32_load_cluster(node, &value_buffer, value_buffer);

    if (error)
      return error;

    /* Reached end? */
    if (value_buffer < 0x2 || value_buffer >= BAD_FAT32)
      break;

    previous_value = value_buffer;
  }

  /* previous_value holds the index to the last cluster at this point. Ez pz */
  *buffer = previous_value;

  return 0;
}

int fat32_write_clusters(oss_node_t* node, uint8_t* buffer, struct fat_file* ffile, uint32_t offset, size_t size)
{
  int error;
  bool did_overflow;
  uint32_t max_offset;
  uint32_t overflow_delta;
  uint32_t overflow_clusters;
  uint32_t write_clusters;
  uint32_t write_start_cluster;
  uint32_t start_cluster_index;
  uint32_t cluster_internal_offset;
  uint32_t file_start_cluster;
  uint32_t file_end_cluster;
  fs_oss_node_t* fs_node;
  fat_fs_info_t* fs_info;
  file_t* file;

  if (!size)
    return -KERR_INVAL;

  file = ffile->parent;

  if (!file)
    return -1;

  fs_node = oss_node_getfs(file->m_obj->parent);
  fs_info = fs_node->m_fs_priv;
  max_offset = ffile->clusters_num * fs_info->cluster_size - 1;
  overflow_delta = 0;

  if (offset + size > max_offset)
    overflow_delta = (offset + size) - max_offset;

  did_overflow = false;

  /* Calculate how many clusters we might need to allocate extra */
  overflow_clusters = ALIGN_UP(overflow_delta, fs_info->cluster_size) / fs_info->cluster_size;
  /* Calculate in how many clusters we need to write */
  write_clusters = ALIGN_UP(size, fs_info->cluster_size) / fs_info->cluster_size;
  /* Calculate the first cluster where we need to write */
  write_start_cluster = ALIGN_DOWN(offset, fs_info->cluster_size) / fs_info->cluster_size;
  /* Calculate the start offset within the first cluster */
  cluster_internal_offset = offset % fs_info->cluster_size;
  /* Find the starting cluster */
  file_start_cluster = __fat32_file_get_start_cluster_entry(ffile);

  /* Find the last cluster of this file */
  error = __fat32_find_end_cluster(node, file_start_cluster, &file_end_cluster);

  if (error)
    return error;

  uint32_t next_free_cluster;

  /* Allocate the overflow clusters */
  while (overflow_clusters) {
    next_free_cluster = NULL;

    /* Find a free cluster */
    error = __fat32_find_free_cluster(node, &next_free_cluster);

    if (error)
      return error;

    /* Add this cluster to the end of this files chain */
    fat32_set_cluster(node, next_free_cluster, file_end_cluster);
    fat32_set_cluster(node, EOF_FAT32, next_free_cluster);

    did_overflow = true;
    file_end_cluster = next_free_cluster;
    overflow_clusters--;
  }

  /* Only re-cache the cluster chain if we made changes to the clusters */
  if (did_overflow) {
    /* Reset cluster_chain data for this file */
    kfree(ffile->clusterchain_buffer);
    ffile->clusterchain_buffer = NULL;
    ffile->clusters_num = NULL;

    /* Re-cache the cluster chain */
    error = fat32_cache_cluster_chain(node, ffile, file_start_cluster);

    if (error)
      return error;

    file->m_total_size = ffile->clusters_num * fs_info->cluster_size;
  }

  /*
   * We can now perform a nice write opperation on this files data
   */
  start_cluster_index = write_start_cluster / ffile->clusters_num;
  uint8_t* cluster_buffer = kmalloc(fs_info->cluster_size);

  /* Current offset: the offset into the current cluster */
  uint32_t current_offset = cluster_internal_offset;
  /* Current delta: how much have we already written */
  uint32_t current_delta = 0;
  /* Write size: how many bytes are in this write */
  uint32_t write_size;
  
  /* FIXME: this is prob slow asf. We could be smarter about our I/O */
  for (uint32_t i = 0; i < write_clusters; i++) {

    if (current_delta >= size)
      break;

    /* This is the cluster we need to write shit to */
    uint32_t this_cluster = ffile->clusterchain_buffer[start_cluster_index+i];

    /* Figure out the write size */
    write_size = size - current_delta;

    /* Write size exceeds the size of our clusters */
    if (write_size > fs_info->cluster_size - current_offset)
      write_size = fs_info->cluster_size - current_offset;

    /* Read the current content into our buffer here */
    fat32_read_clusters(node, cluster_buffer, ffile, this_cluster, 1);

    /* Copy the bytes over */
    memcpy(&cluster_buffer[current_offset], &buffer[current_delta], write_size);

    /* And write back the changes we've made */
    __fat32_write_cluster(node, cluster_buffer, this_cluster);

    current_delta += write_size;
    /* After the first write, any subsequent write will always start at the start of a cluster */
    current_offset = 0;
  }

  fat_dir_entry_t* dir_entry = (fat_dir_entry_t*)((uintptr_t)cluster_buffer + ffile->direntry_offset);

  if (fat32_read_clusters(node, cluster_buffer, ffile, ffile->direntry_cluster, 1))
    goto free_and_exit;

  /* Add the size to this file */
  dir_entry->size += size;

  __fat32_write_cluster(node, cluster_buffer, ffile->direntry_cluster);

free_and_exit:
  kfree(cluster_buffer);
  return 0;
}

/*!
 * @brief: Reads a dir entry from a directory at index @idx
 */
int fat32_read_dir_entry(oss_node_t* node, fat_file_t* dir, fat_dir_entry_t* out, uint32_t idx, uint32_t* diroffset)
{
  size_t dir_entries_count;
  size_t clusters_size;

  fat_fs_info_t* p;

  if (!out)
    return -1;

  if (dir->type != FFILE_TYPE_DIR)
    return -KERR_INVAL;

  if (!dir->clusters_num || !dir->clusterchain_buffer || !dir->dir_entries)
    return -KERR_INVAL;

  /* Get our filesystem info and cluster number */
  p = GET_FAT_FSINFO(node);

  /* Calculate the size of the data */
  clusters_size = dir->clusters_num * p->cluster_size;
  dir_entries_count = clusters_size / sizeof(fat_dir_entry_t);

  /* Outside the range =/ */
  if (idx >= dir_entries_count)
    return -KERR_INVAL;

  *out = dir->dir_entries[idx];

  if (diroffset)
    *diroffset = idx * sizeof(fat_dir_entry_t);

  return 0;
}


static void
transform_fat_filename(char* dest, const char* src) 
{
  uint32_t i = 0;
  uint32_t src_dot_idx = 0;

  bool found_ext = false;

  // put spaces
  memset(dest, ' ', 11);

  while (src[src_dot_idx + i]) {
    // limit exceed
    if (i >= 11 || (i >= 8 && !found_ext)) {
      return;
    }

    // we have found the '.' =D
    if (src[i] == '.') {
      found_ext = true; 
      src_dot_idx = i+1;
      i = 0;
    }

    if (found_ext) {
      dest[8 + i] = TO_UPPERCASE(src[src_dot_idx + i]);
      i++;
    } else {
      dest[i] = TO_UPPERCASE(src[i]);
      i++;
    }
  }
}

static int
fat32_open_dir_entry(oss_node_t* node, fat_dir_entry_t* current, fat_dir_entry_t* out, char* rel_path, uint32_t* diroffset)
{
  int error;
  /* We always use this buffer if we don't support lfn */
  char transformed_buffer[11];
  uint32_t cluster_num;

  size_t dir_entries_count;
  size_t clusters_size;
  fat_dir_entry_t* dir_entries;

  fat_fs_info_t* p;
  fat_file_t tmp_ffile = { 0 };

  if (!out || !rel_path)
    return -1;

  /* Make sure we open a directory, not a file */
  if ((current->attr & FAT_ATTR_DIR) == 0)
    return -2;

  /* Get our filesystem info and cluster number */
  p = GET_FAT_FSINFO(node);
  cluster_num = current->first_cluster_low | ((uint32_t)current->first_cluster_hi << 16);

  /* Cache the cluster chain into the temp file */
  error = fat32_cache_cluster_chain(node, &tmp_ffile, cluster_num);

  if (error)
    goto fail_dealloc_cchain;

  /* Calculate the size of the data */
  clusters_size = tmp_ffile.clusters_num * p->cluster_size;
  dir_entries_count = clusters_size / sizeof(fat_dir_entry_t);

  /* Allocate for that size */
  dir_entries = kmalloc(clusters_size);

  /* Load all the data into the buffer */
  error = fat32_read_clusters(node, (uint8_t*)dir_entries, &tmp_ffile, 0, clusters_size);

  if (error)
    goto fail_dealloc_dir_entries;

  /* Everything is going great, now we can transform the pathname for our scan */
  transform_fat_filename(transformed_buffer, rel_path);

  /* Loop over all the directory entries and check if any paths match ours */
  for (uint32_t i = 0; i < dir_entries_count; i++) {
    fat_dir_entry_t entry = dir_entries[i];

    /* End */
    if (entry.name[0] == 0x00) {
      error = -3;
      break;
    }

    if (entry.attr == FAT_ATTR_LFN) {
      /* TODO: support
       */
      error = -4;
      continue;
    } 

    if (entry.attr & FAT_ATTR_VOLUME_ID)
      continue;

    /* This our file/directory? */
    if (strncmp(transformed_buffer, (const char*)entry.name, 11) == 0) {
      *out = entry;

      /* Give out where we found this bitch */
      if (diroffset)
        *diroffset = i * sizeof(fat_dir_entry_t);

      error = 0;
      break;
    }
  }

fail_dealloc_dir_entries:
  kfree(dir_entries);
fail_dealloc_cchain:
  kfree(tmp_ffile.clusterchain_buffer);
  return error;
}

/* 
 * If this path does not resolve to a file, but does end in a directory, 
 * we should make this file
 */
static oss_obj_t* fat_open(oss_node_t* node, const char* path)
{
  int error;
  file_t* ret;
  fat_fs_info_t* info;
  fat_file_t* fat_file;
  fat_dir_entry_t current;
  uintptr_t current_idx;
  size_t path_size;

  if (!path)
    return nullptr;

  info = GET_FAT_FSINFO(node);
  ret = create_fat_file(info, NULL, path);

  if (!ret)
    return nullptr;

  fat_file = ret->m_private;

  if (!fat_file)
    return nullptr;

  /* Complete the link */
  ret->m_private = fat_file;

  /* Copy the root entry copy =D */
  current = info->root_entry_cpy;
  current_idx = 0;

  path_size = strlen(path) + 1;
  char path_buffer[path_size];

  memcpy(path_buffer, path, path_size);

  for (uintptr_t i = 0; i < path_size; i++) {

    /* Stop either at the end, or at any '/' char */
    if (path_buffer[i] != '/' && path_buffer[i] != '\0')
      continue;

    /* Place a null-byte */
    path_buffer[i] = '\0';

    /* Pre-cache the starting offset of the direntry we're searching in */
    fat_file->direntry_cluster = __fat32_dir_entry_get_start_cluster(&current);

    /* Find the directory entry */
    error = fat32_open_dir_entry(node, &current, &current, &path_buffer[current_idx], &fat_file->direntry_offset);

    /* A single directory may have entries spanning over multiple clusters */
    fat_file->direntry_cluster += fat_file->direntry_offset / info->cluster_size;
    fat_file->direntry_offset %= info->cluster_size;

    if (error)
      break;

    /*
     * If we found our file (its not a directory) we can populate the file object and return it
     */
    if ((current.attr & (FAT_ATTR_DIR|FAT_ATTR_VOLUME_ID)) != FAT_ATTR_DIR) {

      /* Make sure the file knows about its cluster chain */
      error = fat32_cache_cluster_chain(node, fat_file, (current.first_cluster_low | ((uint32_t)current.first_cluster_hi << 16)));

      if (error)
        break;

      /* This is quite aggressive, we should prob just clean and return nullptr... */
      ASSERT_MSG(!oss_attach_obj_rel(node, path, ret->m_obj), "Failed to attach FAT object");

      ret->m_total_size = get_fat_file_size(fat_file);
      ret->m_logical_size = current.size;

      /* Cache the offset of the clusterchain */
      fat_file->clusterchain_offset = (current.first_cluster_low | ((uint32_t)current.first_cluster_hi << 16));

      return ret->m_obj;
    }

    /* Set the current index if we have successfuly 'switched' directories */
    current_idx = i + 1;

    /*
     * Place back the slash
     */
    path_buffer[i] = '/';
  }

  /* NOTE: destroy the vobj, which will also destroy the file obj */
  destroy_oss_obj(ret->m_obj);
  return nullptr;
}

static oss_node_t* fat_open_dir(oss_node_t* node, const char* path)
{
  int error;
  dir_t* ret;
  fat_fs_info_t* info;
  fat_file_t* fat_file;
  fat_dir_entry_t current;
  uintptr_t current_idx;
  size_t path_size;

  if (!path)
    return nullptr;

  info = GET_FAT_FSINFO(node);
  ret = create_fat_dir(info, NULL, path);

  if (!ret)
    return nullptr;

  fat_file = ret->priv;

  if (!fat_file)
    return nullptr;

  /* Complete the link */
  ret->priv = fat_file;

  /* Copy the root entry copy =D */
  current = info->root_entry_cpy;
  current_idx = 0;

  path_size = strlen(path) + 1;
  char path_buffer[path_size];

  memcpy(path_buffer, path, path_size);

  for (uintptr_t i = 0; i < path_size; i++) {

    /* Stop either at the end, or at any '/' char */
    if (path_buffer[i] != '/' && path_buffer[i] != '\0')
      continue;

    /* Place a null-byte */
    path_buffer[i] = '\0';

    /* Pre-cache the starting offset of the direntry we're searching in */
    fat_file->direntry_cluster = __fat32_dir_entry_get_start_cluster(&current);

    /* Find the directory entry */
    error = fat32_open_dir_entry(node, &current, &current, &path_buffer[current_idx], &fat_file->direntry_offset);

    /* A single directory may have entries spanning over multiple clusters */
    fat_file->direntry_cluster += fat_file->direntry_offset / info->cluster_size;
    fat_file->direntry_offset %= info->cluster_size;

    if (error)
      break;

    /* Set the current index if we have successfuly 'switched' directories */
    current_idx = i + 1;

    /*
     * If we found our file (its not a directory) we can populate the file object and return it
     */
    if ((current.attr & (FAT_ATTR_DIR)) == FAT_ATTR_DIR && (current_idx >= path_size || path_buffer[current_idx] == NULL)) {

      /* Make sure the file knows about its cluster chain */
      error = fat32_cache_cluster_chain(node, fat_file, (current.first_cluster_low | ((uint32_t)current.first_cluster_hi << 16)));

      if (error)
        break;

      /* This is quite aggressive, we should prob just clean and return nullptr... */
      //ASSERT_MSG(!oss_attach_node(path, ret->node), "Failed to attach FAT node");

      /* Update the directory entries for this directory file */
      error = fat_file_update_dir_entries(fat_file);

      if (error)
        break;

      ret->size = get_fat_file_size(fat_file);
      ret->child_capacity = ret->size / sizeof(fat_dir_entry_t);

      printf("Opening FAT dir object: %p (size=%d, child_capacity=%d)\n", ret, ret->size, ret->child_capacity);

      /* Cache the offset of the clusterchain */
      fat_file->clusterchain_offset = (current.first_cluster_low | ((uint32_t)current.first_cluster_hi << 16));

      return ret->node;
    }

    /*
     * Place back the slash
     */
    path_buffer[i] = '/';
  }

  destroy_dir(ret);
  return nullptr;
}

static int fat_close(oss_node_t* node, oss_obj_t* obj)
{
  return 0;
}

static int fat_msg(oss_node_t* node, dcc_t code, void* buffer, size_t size)
{
  return 0;
}

static int fat_destroy(oss_node_t* node)
{
  kernel_panic("TODO: fat_destroy");
  return 0;
}

static struct oss_node_ops fat_node_ops = {
  .f_msg = fat_msg,
  .f_open = fat_open,
  .f_open_node = fat_open_dir,
  .f_close = fat_close,
  .f_destroy = fat_destroy,
};

/*
 * FAT will keep a list of vobjects that have been given away by the system, in order to map
 * vobjects to disk-locations and FATs easier.
 */
oss_node_t* fat32_mount(fs_type_t* type, const char* mountpoint, partitioned_disk_dev_t* device) {

  /* Get FAT =^) */
  fat_fs_info_t* ffi;
  uint8_t buffer[device->m_parent->m_logical_sector_size];
  fat_boot_sector_t* boot_sector;
  fat_boot_fsinfo_t* internal_fs_info;
  oss_node_t* node;

  KLOG_DBG("Trying to mount FAT32\n");

  int read_result = pd_bread(device, buffer, 0);

  if (read_result < 0)
    return nullptr;

  /* Create root node */
  node = create_fs_oss_node(mountpoint, type, &fat_node_ops);

  ASSERT_MSG(node, "Failed to create fs oss node for FAT fs");

  /* Initialize the fs private info */
  create_fat_info(node, device);

  /* Cache superblock and fs info */
  ffi = GET_FAT_FSINFO(node);

  /* Set the local pointer to the bootsector */
  boot_sector = &ffi->boot_sector_copy;

  /*
   * Copy the boot sector cuz why not lol 
   * NOTE: the only access the boot sector after this point lmao
   */
  memcpy(boot_sector, buffer, sizeof(fat_boot_sector_t));

  /*
   * Create a cache for our sectors 
   * NOTE: cache_count being NULL means we want the default number of cache entries
   */
  ffi->sector_cache = create_fat_sector_cache(device->m_parent->m_effective_sector_size, NULL);

  /* Try to parse boot sector */
  int parse_result = parse_fat_bpb(boot_sector, node);

  /* Not a valid FAT fs */
  if (parse_result != FAT_OK)
    goto fail;

  /* Check FAT type */
  if (!boot_sector->sectors_per_fat16 && boot_sector->fat32.sectors_per_fat) {
    /* FAT32 */
    ffi->fat_type = FTYPE_FAT32;
  } else {
    /* FAT12 or FAT16 */
    ffi->fat_type = FTYPE_FAT16;
    goto fail;
  }

  /* Attempt to reset the blocksize for the partitioned device */
  if (device->m_parent->m_logical_sector_size > oss_node_getfs(node)->m_blocksize) {

    if (!pd_set_blocksize(device, oss_node_getfs(node)->m_blocksize))
      kernel_panic("Failed to set blocksize! abort");
  }

  /* Did a previous OS mark this filesystem as dirty? */
  ffi->has_dirty = is_fat32(ffi) ? (boot_sector->fat32.state & FAT_STATE_DIRTY) : (boot_sector->fat16.state & FAT_STATE_DIRTY);

  /* TODO: move superblock initialization to its own function */

  /* Fetch blockcounts */
  oss_node_getfs(node)->m_total_blocks = ffi->boot_sector_copy.sector_count_fat16;

  if (!oss_node_getfs(node)->m_total_blocks)
    oss_node_getfs(node)->m_total_blocks = ffi->boot_sector_copy.sector_count_fat32;

  oss_node_getfs(node)->m_first_usable_block = ffi->boot_sector_copy.reserved_sectors;

  /* Precompute some useful data (TODO: compute based on FAT type)*/
  ffi->root_directory_sectors = (boot_sector->root_dir_entries * 32 + (boot_sector->sector_size - 1)) / boot_sector->sector_size;
  ffi->total_reserved_sectors = boot_sector->reserved_sectors + (boot_sector->fats * boot_sector->fat32.sectors_per_fat) + ffi->root_directory_sectors;
  ffi->total_usable_sectors = oss_node_getfs(node)->m_total_blocks - ffi->total_reserved_sectors; 
  ffi->cluster_count = ffi->total_usable_sectors / boot_sector->sectors_per_cluster;
  ffi->cluster_size = boot_sector->sectors_per_cluster * boot_sector->sector_size;
  ffi->usable_sector_offset = boot_sector->reserved_sectors * boot_sector->sector_size;
  ffi->usable_clusters_start = (boot_sector->reserved_sectors + (boot_sector->fats * boot_sector->fat32.sectors_per_fat));

  ffi->root_entry_cpy = (fat_dir_entry_t) {
    .name = "/",
    .first_cluster_low = boot_sector->fat32.root_cluster & 0xffff,
    .first_cluster_hi = (boot_sector->fat32.root_cluster >> 16) & 0xffff,
    .attr = FAT_ATTR_DIR,
    0
  };

  /* NOTE: we reuse the buffer of the boot sector */
  read_result = pd_bread(device, buffer, 1);

  if (read_result < 0)
    goto fail;

  /* Set the local pointer */
  internal_fs_info = &ffi->boot_fs_info;

  /* Copy the boot fsinfo to the ffi structure */
  memcpy(internal_fs_info, buffer, sizeof(fat_boot_fsinfo_t));

  oss_node_getfs(node)->m_free_blocks = ffi->boot_fs_info.free_clusters;

  return node;
fail:

  if (node) {
    destroy_fat_info(node);
    /* NOTE: total node destruction is not yet done */
    destroy_fs_oss_node(node);
  }

  return nullptr;
}

/*!
 * @brief: Unmount the FAT filesystem
 * 
 * This simply needs to free up the private structures we created for this node
 * to hold the information about the FAT filesystem. We don't need to wory about fat
 * files here, since they will be automatically killed when the filesystem is destroyed
 *
 * NOTE: An unmount should happen after a total filesystem sync, to prevent loss of data
 */
int fat32_unmount(fs_type_t* type, oss_node_t* node) 
{
  if (!type || !node)
    return -1;

  destroy_fat_info(node);
  return 0;
}

aniva_driver_t fat32_drv = {
  .m_name = "fat32",
  .m_type = DT_FS,
  .f_init = fat32_init,
  .f_exit = fat32_exit,
};
EXPORT_DRIVER_PTR(fat32_drv);

fs_type_t fat32_type = {
  .m_driver = &fat32_drv,
  .m_name = "fat32",
  .f_mount = fat32_mount,
  .f_unmount = fat32_unmount,
};

int fat32_init() 
{
  ErrorOrPtr result;

  init_fat_cache();

  result = register_filesystem(&fat32_type);

  if (result.m_status == ANIVA_FAIL) {
    return -1;
  }

  return 0;
}

int fat32_exit() {

  ErrorOrPtr result = unregister_filesystem(&fat32_type);

  if (result.m_status == ANIVA_FAIL)
    return -1;

  return 0;
}
