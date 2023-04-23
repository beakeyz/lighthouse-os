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

  if (bpb->fat32.sectors_per_fat == 0 && bpb->sectors_per_fat == 0) {
    return FAT_INVAL;
  }

  /* TODO: fill data in superblock with useful data */


  return FAT_OK;
}

vnode_t* fat32_mount(fs_type_t* type, const char* mountpoint, partitioned_disk_dev_t* device) {

  /* Get FAT =^) */

  fat_fs_info_t* ffi;
  uint8_t bpb_buffer[device->m_parent->m_logical_sector_size];
  fat_boot_sector_t* boot_sector = (fat_boot_sector_t*)bpb_buffer;

  int read_result = read_sync_partitioned_blocks(device, &bpb_buffer, device->m_parent->m_logical_sector_size, 0);

  if (read_result) {
    return nullptr;
  }

  device->m_superblock = create_fs_superblock();
  ffi = create_fat_fs_info();

  if (!ffi)
    return nullptr;

  device->m_superblock->m_fs_specific_info = ffi;

  int parse_result = parse_fat_bpb(boot_sector, device->m_superblock);

  if (parse_result != FAT_OK)
    return nullptr;

  if (!boot_sector->sectors_per_fat && boot_sector->fat32.sectors_per_fat) {
    /* FAT32 */
  } else {
    /* FAT12 or FAT16 */
  }

  vnode_t* node = create_generic_vnode("fat", VN_FS | VN_ROOT);
  kernel_panic("Test");

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
