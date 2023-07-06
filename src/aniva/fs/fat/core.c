#ifndef __ANIVA_FS_FAT32__
#define __ANIVA_FS_FAT32__

#include "dev/debug/serial.h"
#include "dev/disk/generic.h"
#include "dev/driver.h"
#include "fs/core.h"
#include "fs/fat/cache.h"
#include "fs/fat/core.h"
#include "fs/namespace.h"
#include "fs/superblock.h"
#include "fs/vnode.h"
#include "fs/vobj.h"
#include "libk/flow/error.h"
#include "libk/math/log2.h"
#include "libk/string.h"
#include <libk/stddef.h>

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
static int fat_parse_lfn(void* dir, fat_offset_t* offset);

static int fat_parse_short_fn(fs_superblock_t* sb, fat_dir_entry_t* dir, const char* name);

static int parse_fat_bpb(fat_boot_sector_t* bpb, fs_superblock_t* block) {

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
  block->m_blocksize = bpb->sector_size;

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

  fs_superblock_t* sb;
  fat_fs_info_t* ffi;
  uint8_t buffer[device->m_parent->m_logical_sector_size];
  fat_boot_sector_t* boot_sector = (fat_boot_sector_t*)buffer;
  fat_boot_fsinfo_t* fat_boot_fs_info;
  vnode_t* node;

  int read_result = pd_bread(device, buffer, 0);

  if (read_result < 0) {
    return nullptr;
  }

  /* Create sb */
  device->m_superblock = create_fs_superblock();

  /* Create root vnode */
  node = create_generic_vnode(mountpoint, VN_FS | VN_ROOT);

  /* Set the vnode device */
  node->m_device = device;

  create_fat_info(node);

  /* Cache superblock and fs info */
  sb = device->m_superblock;
  ffi = device->m_superblock->m_fs_specific_info;

  /* Copy the boot sector cuz why not lol */
  ffi->boot_sector_copy = *boot_sector;

  int parse_result = parse_fat_bpb(boot_sector, device->m_superblock);

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

  if (device->m_parent->m_logical_sector_size > sb->m_blocksize) {

    if (!pd_set_blocksize(device, sb->m_blocksize)) {
      kernel_panic("Failed to set blocksize! abort");
    }
  }

  ffi->is_dirty = is_fat32(ffi) ? (boot_sector->fat32.state & FAT_STATE_DIRTY) : (boot_sector->fat16.state & FAT_STATE_DIRTY);

  device->m_superblock->m_total_blocks = ffi->boot_sector_copy.sectors;

  if (!device->m_superblock->m_total_blocks)
    device->m_superblock->m_total_blocks = ffi->boot_sector_copy.total_sectors;

  device->m_superblock->m_first_usable_block = ffi->boot_sector_copy.reserved_sectors;

  /* NOTE: we reuse the buffer of the boot sector */
  read_result = pd_bread(device, buffer, 1);

  if (read_result < 0)
    goto fail;

  fat_boot_fs_info = (fat_boot_fsinfo_t*)buffer;

  ffi->boot_fs_info = *fat_boot_fs_info;

  device->m_superblock->m_free_blocks = ffi->boot_fs_info.free_clusters;

  println(to_string(device->m_superblock->m_total_blocks));
  println(to_string(device->m_superblock->m_free_blocks));

  //kernel_panic("Test");

  return nullptr;

fail:

  if (node) {
    destroy_fat_info(node);
    destroy_generic_vnode(node);
  }

  /* Make sure the devices superblock is killed before we use this pointer again */
  if (device->m_superblock)
    destroy_fs_superblock(device->m_superblock);


  device->m_superblock = nullptr;

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
