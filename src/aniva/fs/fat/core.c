#ifndef __ANIVA_FS_FAT32__
#define __ANIVA_FS_FAT32__

#include "dev/core.h"
#include "dev/debug/serial.h"
#include "dev/disk/generic.h"
#include "dev/driver.h"
#include "fs/core.h"
#include "fs/fat/cache.h"
#include "fs/fat/core.h"
#include "fs/file.h"
#include "fs/namespace.h"
#include "fs/vnode.h"
#include "fs/vobj.h"
#include "libk/flow/error.h"
#include "libk/math/log2.h"
#include "libk/string.h"
#include "logging/log.h"
#include <libk/stddef.h>

/* Driver predefinitions */
int fat32_exit();
int fat32_init();

enum FAT_ERRCODES {
  FAT_OK = 0,
  FAT_INVAL = 1,
};

/* Following ASCII */
static inline uint8_t fat_to_lower(char c) {
  return ((c >= 'A') && (c <= 'Z')) ? c+32 : c;
}

#define TO_UPPERCASE(c) ((c >= 'a' && c <= 'z') ? c - 0x20 : c)
#define TO_LOWERCASE(c) ((c >= 'A' && c <= 'Z') ? c + 0x20 : c)

typedef uintptr_t fat_offset_t;

/* TODO: */
//static int fat_parse_lfn(void* dir, fat_offset_t* offset);
//static int fat_parse_short_fn(fat_dir_entry_t* dir, const char* name);

static int parse_fat_bpb(fat_boot_sector_t* bpb, vnode_t* block) {

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

  if (bpb->sectors_per_fat == 0 && bpb->fat32.sectors_per_fat == 0) {
    return FAT_INVAL;
  }

  /* TODO: fill data in superblock with useful data */
  block->fs_data.m_blocksize = bpb->sector_size;

  return FAT_OK;
}

/*!
 * @brief Load a singular clusters value
 *
 * fs: the filesystem object to opperate on
 * buffer: the buffer for the clusters value
 * cluster: the 'index' of the cluster on disk
 */
static int
__fat32_load_cluster(vnode_t* node, uint32_t* buffer, uint32_t cluster)
{
  int error;
  fat_fs_info_t* info;

  info = VN_FS_DATA(node).m_fs_specific_info;
  error = fat_read(node, buffer, sizeof(uint32_t), info->usable_sector_offset + (cluster * sizeof(uint32_t)));

  if (error)
    return error;

  /* Mask any fucky bits to comply with FAT32 standards */
  *buffer &= 0x0fffffff;

  return 0;
}

static int
__fat32_cache_cluster_chain(vnode_t* node, fat_file_t* file, uint32_t start_cluster)
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

  println("Counting");
  /* Count the clusters */
  while (true) {
    error = __fat32_load_cluster(node, &buffer, buffer);

    if (error)
      return error;

    if (buffer < 0x2 || buffer > FAT_ENT_EOF)
      break;

    length++;
  }

  /* Reset buffer */
  buffer = start_cluster;

  /* Allocate chain */
  file->clusterchain_buffer = kmalloc(length * sizeof(uint32_t));

  /* Set correct lengths */
  file->clusterchain_size = length;

  println("Stroring");
  /* Loop and store the clusters */
  for (uint32_t i = 0; i < length; i++) {
    file->clusterchain_buffer[i] = buffer;

    error = __fat32_load_cluster(node, &buffer, buffer);

    if (error)
      return error;
  }
  
  return 0;
}

static int
__fat32_load_clusters(vnode_t* node, void* buffer, fat_file_t* file, uint32_t start, size_t count)
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

  info = VN_FS_DATA(node).m_fs_specific_info;

  index = 0;

  while (index < count) {
    current_offset = start + index;
    current_index = current_offset / info->cluster_size;
    current_deviation = current_offset % info->cluster_size;

    current_delta = count - index;

    /* Limit the delta to the size of a cluster, except if we try to get an unaligned size smaller than 'cluster_size' */
    if (current_delta > info->cluster_size - current_deviation)
      current_delta = info->cluster_size - current_deviation;

    current_cluster_offset = (info->usable_clusters_start + (file->clusterchain_buffer[current_index] - 2) * info->boot_sector_copy.sectors_per_cluster) * info->boot_sector_copy.sector_size;

    error = fat_read(node, buffer + index, current_delta, current_cluster_offset + current_deviation);

    if (error)
      return -2;

    index += current_delta;
  }

  return 0;
}

static int
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
      return 0;
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

  return 0;
}

static int 
__fat32_open_dir_entry(vnode_t* node, fat_dir_entry_t* current, fat_dir_entry_t* out, char* rel_path)
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

  println("A");
  if (!out || !rel_path)
    return -1;

  /* Make sure we open a directory, not a file */
  if ((current->attr & FAT_ATTR_DIR) == 0)
    return -2;

  println("shit");
  p = VN_FS_DATA(node).m_fs_specific_info;
  cluster_num = current->first_clutser_low | ((uint32_t)current->first_cluster_hi << 16);

  error = __fat32_cache_cluster_chain(node, &tmp_ffile, cluster_num);

  if (error)
    goto fail_dealloc_cchain;

  println("bro");
  clusters_size = tmp_ffile.clusterchain_size * p->cluster_size;
  dir_entries_count = clusters_size / sizeof(fat_dir_entry_t);

  dir_entries = kmalloc(clusters_size);

  println("L");
  error = __fat32_load_clusters(node, dir_entries, &tmp_ffile, 0, clusters_size);

  if (error)
    goto fail_dealloc_dir_entries;

  println("Dic");
  error = transform_fat_filename(transformed_buffer, rel_path);

  /* Loop over all the directory entries and check if any paths match ours */
  for (uint32_t i = 0; i < dir_entries_count; i++) {
    fat_dir_entry_t entry = dir_entries[i];

    println((char*)entry.name);

    /* Space kinda cringe */
    if (entry.name[0] == ' ')
      continue;

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
    } else {

      /* This our file/directory? */
      if (strncmp(transformed_buffer, (const char*)entry.name, 11) == 0) {
        *out = entry;
        error = 0;
        break;
      }
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
static vobj_t* fat_open(vnode_t* node, char* path)
{
  int error;
  file_t* ret;
  fat_fs_info_t* info;
  fat_file_t* fat_file;
  fat_dir_entry_t current;
  uintptr_t current_idx;
  size_t path_size;

  println("fat_open");
  if (!path)
    return nullptr;

  info = VN_FS_DATA(node).m_fs_specific_info;
  ret = create_file(node, NULL, path);

  println("file");
  if (!ret)
    return nullptr;

  println("fat file");
  fat_file = create_fat_file(ret);

  if (!ret)
    return nullptr;

  /* Complete the link */
  ret->m_private = fat_file;

  /* Copy the root entry copy =D */
  current = info->root_entry_cpy;
  current_idx = 0;

  path_size = strlen(path) + 1;
  char path_buffer[path_size];

  memcpy(path_buffer, path, path_size);

  println(path);

  for (uintptr_t i = 0; i < path_size; i++) {

    /* Stop either at the end, or at any '/' char */
    if (path_buffer[i] != '/' && path_buffer[i] != '\0')
      continue;

    /* Place a null-byte */
    path_buffer[i] = '\0';

    println(path_buffer);

    error = __fat32_open_dir_entry(node, &current, &current, &path_buffer[current_idx]);

    if (error)
      break;

    /*
     * If we found our file (its not a directory) we can populate the file object and return it
     */
    if ((current.attr & (FAT_ATTR_DIR|FAT_ATTR_VOLUME_ID)) != FAT_ATTR_DIR) {

      /* Make sure the file knows about its cluster chain */
      error = __fat32_cache_cluster_chain(node, fat_file, (current.first_clutser_low | (current.first_cluster_hi << 16)));

      if (error)
        break;

      return ret->m_obj;
    }

    /* Set the current index if we have successfuly 'switched' directories */
    current_idx = i + 1;

    /*
     * Place back the slash
     */
    path_buffer[i] = '/';
  }

  destroy_fat_file(fat_file);
  /* NOTE: destroy the vobj, which will also destroy the file obj */
  destroy_vobj(ret->m_obj);
  return nullptr;
}

static int fat_close(vnode_t* node, vobj_t* obj)
{
  return -1;
}

static int fat_mkdir(struct vnode* node, void* dir_entry, void* dir_attr, uint32_t flags)
{
  return -1;
}

static int fat_msg(vnode_t* node, dcc_t code, void* buffer, size_t size)
{
  return 0;
}

static struct generic_vnode_ops __fat_vnode_ops = {
  .f_open = fat_open,
  .f_close = fat_close,
  .f_makedir = fat_mkdir,
  /* TODO */
  .f_rmdir = nullptr,
  .f_msg = fat_msg,
};

/*
 * FAT will keep a list of vobjects that have been given away by the system, in order to map
 * vobjects to disk-locations and FATs easier.
 */
vnode_t* fat32_mount(fs_type_t* type, const char* mountpoint, partitioned_disk_dev_t* device) {

  /* Get FAT =^) */

  fat_fs_info_t* ffi;
  uint8_t buffer[device->m_parent->m_logical_sector_size];
  fat_boot_sector_t* boot_sector;
  fat_boot_fsinfo_t* internal_fs_info;
  vnode_t* node;

  int read_result = pd_bread(device, buffer, 0);

  if (read_result < 0)
    return nullptr;

  /* Create root vnode */
  node = create_generic_vnode(mountpoint, VN_FS | VN_ROOT);

  /* Set the vnode device */
  node->m_device = device;

  create_fat_info(node);

  /* Cache superblock and fs info */
  ffi = VN_FS_DATA(node).m_fs_specific_info;

  /* Set the local pointer to the bootsector */
  boot_sector = &ffi->boot_sector_copy;

  /*
   * Copy the boot sector cuz why not lol 
   * NOTE: the only access the boot sector after this point lmao
   */
  memcpy(boot_sector, buffer, sizeof(fat_boot_sector_t));

  /* Create a cache for our sectors */
  ffi->sector_cache = create_fat_sector_cache(boot_sector->sector_size);

  /* Try to parse boot sector */
  int parse_result = parse_fat_bpb(boot_sector, node);

  /* Not a valid FAT fs */
  if (parse_result != FAT_OK)
    goto fail;

  /* Check FAT type */
  if (!boot_sector->sectors_per_fat && boot_sector->fat32.sectors_per_fat) {
    /* FAT32 */
    ffi->fat_type = FTYPE_FAT32;
  } else {
    /* FAT12 or FAT16 */
    ffi->fat_type = FTYPE_FAT16;
    goto fail;
  }

  /* Attempt to reset the blocksize for the partitioned device */
  if (device->m_parent->m_logical_sector_size > VN_FS_DATA(node).m_blocksize) {

    if (!pd_set_blocksize(device, VN_FS_DATA(node).m_blocksize)) {
      kernel_panic("Failed to set blocksize! abort");
    }
  }

  /* Did a previous OS mark this filesystem as dirty? */
  ffi->has_dirty = is_fat32(ffi) ? (boot_sector->fat32.state & FAT_STATE_DIRTY) : (boot_sector->fat16.state & FAT_STATE_DIRTY);

  /* TODO: move superblock initialization to its own function */

  /* Fetch blockcounts */
  VN_FS_DATA(node).m_total_blocks = ffi->boot_sector_copy.sectors;

  if (!VN_FS_DATA(node).m_total_blocks)
    VN_FS_DATA(node).m_total_blocks = ffi->boot_sector_copy.total_sectors;

  VN_FS_DATA(node).m_first_usable_block = ffi->boot_sector_copy.reserved_sectors;

  /* Precompute some useful data */
  ffi->root_directory_sectors = (boot_sector->root_dir_entries * 32 + (boot_sector->sector_size - 1)) / boot_sector->sector_size;
  ffi->total_reserved_sectors = boot_sector->reserved_sectors + (boot_sector->fats * boot_sector->sectors_per_fat) + ffi->root_directory_sectors;
  ffi->total_usable_sectors = VN_FS_DATA(node).m_total_blocks - ffi->total_reserved_sectors; 
  ffi->cluster_count = ffi->total_usable_sectors / boot_sector->sectors_per_cluster;
  ffi->cluster_size = boot_sector->sectors_per_cluster * boot_sector->sector_size;
  ffi->usable_sector_offset = boot_sector->reserved_sectors * boot_sector->sector_size;
  ffi->usable_clusters_start = (uint64_t)(boot_sector->reserved_sectors + (boot_sector->fats * boot_sector->sectors_per_fat));

  ffi->root_entry_cpy = (fat_dir_entry_t) {
    .name = "/",
    .first_clutser_low = boot_sector->fat32.root_cluster & 0xffff,
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

  VN_FS_DATA(node).m_free_blocks = ffi->boot_fs_info.free_clusters;

  node->m_ops = &__fat_vnode_ops;

  println("Did fat filesystem!");

  //kernel_panic("Test");

  node->m_flags |= VN_FLEXIBLE;

  return node;
fail:

  if (node) {
    destroy_fat_info(node);
    /* NOTE: total vnode destruction is not yet done */
    destroy_generic_vnode(node);
  }

  return nullptr;
}

aniva_driver_t fat32_drv = {
  .m_name = "fat32",
  .m_type = DT_FS,
  .f_init = fat32_init,
  .f_exit = fat32_exit,
  .m_dependencies = {"disk/ahci"},
  .m_dep_count = 1,
};
EXPORT_DRIVER_PTR(fat32_drv);

fs_type_t fat32_type = {
  .m_driver = &fat32_drv,
  .m_name = "fat32",
  .f_mount = fat32_mount,
};

int fat32_init() {

  println("Initialized fat32 driver");
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


#endif /* ifndef __ANIVA_FS_FAT32__ */
