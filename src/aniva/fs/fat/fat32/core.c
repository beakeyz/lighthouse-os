#ifndef __ANIVA_FS_FAT32__
#define __ANIVA_FS_FAT32__

#include "dev/debug/serial.h"
#include "dev/driver.h"
#include "fs/core.h"
#include "fs/vnode.h"
#include <libk/stddef.h>

int fat32_exit();
int fat32_init();


vnode_t* fat32_mount(fs_type_t* type, const char* mountpoint, partitioned_disk_dev_t* device) {

  /* Get FAT =^) */

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
