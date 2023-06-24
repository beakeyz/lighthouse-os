#ifndef __ANIVA_FS_FAT32__
#define __ANIVA_FS_FAT32__

#include "dev/debug/serial.h"
#include "dev/disk/generic.h"
#include "dev/driver.h"
#include "fs/core.h"
#include "fs/fat/generic.h"
#include "fs/namespace.h"
#include "fs/superblock.h"
#include "fs/vnode.h"
#include "libk/error.h"
#include "libk/log2.h"
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

static inline fat_offset_t fat_make_offset(fs_superblock_t* sb, uintptr_t block_nr, fat_dir_entry_t* de0, fat_dir_entry_t* de1);

static ErrorOrPtr fat_get_dir_entry_heavy(vnode_t* n, uintptr_t* offset, fat_dir_entry_t** dir_entry);
ErrorOrPtr fat_get_dir_entry(vnode_t* node, uintptr_t* offset, fat_dir_entry_t** dir_entry);

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

vnode_t* fat32_mount(fs_type_t* type, const char* mountpoint, partitioned_disk_dev_t* device) {

  /* Get FAT =^) */

  fs_superblock_t* sb;
  fat_fs_info_t* ffi;
  uint8_t bpb_buffer[device->m_parent->m_logical_sector_size];
  fat_boot_sector_t* boot_sector = (fat_boot_sector_t*)bpb_buffer;

  int read_result = pd_bread(device, &bpb_buffer, 0);

  if (read_result < 0) {
    return nullptr;
  }

  device->m_superblock = create_fs_superblock();

  ffi = create_fat_fs_info();
  sb = device->m_superblock;

  if (!ffi)
    return nullptr;

  device->m_superblock->m_fs_specific_info = ffi;

  /* Copy the boot sector cuz why not lol */
  ffi->boot_sector_copy = *boot_sector;

  int parse_result = parse_fat_bpb(boot_sector, device->m_superblock);

  if (parse_result != FAT_OK)
    return nullptr;

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

  /* TODO: */
  vnode_t* node = create_generic_vnode(mountpoint, VN_FS | VN_ROOT);
  println(to_string(ffi->fat_type));
  println(to_string(boot_sector->sectors_per_fat));
  println(to_string(boot_sector->sectors));
  println(to_string(boot_sector->media));
  println(to_string(boot_sector->dir_entries));
  println(to_string(boot_sector->sector_size));
  println(to_string(boot_sector->total_sectors));
  kernel_panic("Test");

  return node;
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
