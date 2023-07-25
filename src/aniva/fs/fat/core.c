#ifndef __ANIVA_FS_FAT32__
#define __ANIVA_FS_FAT32__

#include "dev/debug/serial.h"
#include "dev/disk/generic.h"
#include "dev/driver.h"
#include "fs/core.h"
#include "fs/fat/cache.h"
#include "fs/fat/core.h"
#include "fs/namespace.h"
#include "fs/vnode.h"
#include "fs/vobj.h"
#include "libk/flow/error.h"
#include "libk/math/log2.h"
#include "libk/string.h"
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

/* 
 * If this path does not resolve to a file, but does end in a directory, 
 * we should make this file
 */
static vobj_t* fat_open(vnode_t* node, char* path)
{
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

static struct generic_vnode_ops __fat_vnode_ops = {
  .f_open = fat_open,
  .f_close = fat_close,
  .f_makedir = fat_mkdir,
  /* TODO */
  .f_rmdir = nullptr,
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

  if (read_result < 0) {
    return nullptr;
  }

  /* Create root vnode */
  node = create_generic_vnode(mountpoint, VN_FS | VN_ROOT);

  /* Set the vnode device */
  node->m_device = device;

  create_fat_info(node);

  /* Cache superblock and fs info */
  ffi = VN_FS_DATA(node).m_fs_specific_info;

  /* Set the local pointer to the bootsector */
  boot_sector = &ffi->boot_sector_copy;

  /* Copy the boot sector cuz why not lol */
  memcpy(boot_sector, buffer, sizeof(fat_boot_sector_t));

  /* Try to parse boot sector */
  int parse_result = parse_fat_bpb(boot_sector, node);

  /* Not a valid FAT fs */
  if (parse_result != FAT_OK)
    return nullptr;

  /* Check FAT type */
  if (!boot_sector->sectors_per_fat && boot_sector->fat32.sectors_per_fat) {
    /* FAT32 */
    ffi->fat_type = FTYPE_FAT32;
  } else {
    /* FAT12 or FAT16 */
    ffi->fat_type = FTYPE_FAT16;
  }

  /* Compute total size */
  ffi->total_fs_size = boot_sector->sector_size * boot_sector->total_sectors;

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

  /* NOTE: we reuse the buffer of the boot sector */
  read_result = pd_bread(device, buffer, 1);

  if (read_result < 0)
    goto fail;

  /* Set the local pointer */
  internal_fs_info = &ffi->boot_fs_info;

  /* Copy the boot fsinfo to the ffi structure */
  memcpy(internal_fs_info, buffer, sizeof(fat_boot_fsinfo_t));

  VN_FS_DATA(node).m_free_blocks = ffi->boot_fs_info.free_clusters;

  fat_prepare_finfo(node);

  node->m_ops = &__fat_vnode_ops;

  println("Did fat filesystem!");

  //kernel_panic("Test");

  return nullptr;

fail:

  if (node) {
    destroy_fat_info(node);
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
EXPORT_DRIVER(fat32_drv);

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
