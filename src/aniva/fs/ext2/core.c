#include "dev/core.h"
#include "dev/debug/serial.h"
#include "dev/disk/generic.h"
#include "dev/driver.h"
#include "fs/ext2/core.h"
#include "mem/heap.h"
#include <libk/stddef.h>
#include <libk/string.h>
#include <libk/error.h>
#include <fs/core.h>
#include <fs/vnode.h>

int ext2_init();
int ext2_exit();

typedef struct ext2_info {
  uint32_t blocksize;
  uint32_t inodes_per_group;
  uint32_t inode_size;
  uint32_t blocks_group_count;
} ext2_info_t;

struct ext2 {

  struct ext2_superblock*               m_superblock;
  struct ext2_block_group_descriptor*   m_block_group_descriptor;

  partitioned_disk_dev_t*               m_device;

  /* stats of this ext2 filesystem */
  ext2_info_t                           m_info;
  
  fs_ops_t                              m_ops;
};

struct ext2_superblock* fetch_superblock();

vnode_t* ext2_mount(fs_type_t* type, const char* mountpoint, partitioned_disk_dev_t* device) {

  ASSERT_MSG(device, "Can't initialize ext2 fs without a disk device");

  ext2_superblock_t* superblock = kmalloc(sizeof(ext2_superblock_t));

  read_sync_partitioned_block(device, superblock, sizeof(ext2_superblock_t), 1);

  // TODO: =)

  return 0;
}

/*
 * TODO: make disk usage a path of least resistance instead of just picking out something.
 * If there is no ahci driver, we would be fucked in this case lol
 */
aniva_driver_t ext2_drv = {
  .m_name = "ext2",
  .m_type = DT_FS,
  .f_init = ext2_init,
  .f_exit = ext2_exit,
  .m_dependencies = {"disk/ahci"},
  .m_dep_count = 1,
};
EXPORT_DRIVER(ext2_drv);

fs_type_t ext2_type = {
  .m_driver = &ext2_drv,
  .m_name = "ext2",
  .f_mount = ext2_mount,
};

int ext2_init() {

  println("Initialized ext2 driver");
  ErrorOrPtr result;

  result = register_filesystem(&ext2_type);

  if (result.m_status == ANIVA_FAIL)
    return -1;

  // kernel_panic("Registered the ext2 filesystem...");
  return 0;
}

int ext2_exit() {

  ErrorOrPtr result = unregister_filesystem(&ext2_type);

  if (result.m_status == ANIVA_FAIL)
    return -1;

  return 0;
}

