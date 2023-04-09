#include "dev/core.h"
#include "dev/debug/serial.h"
#include "dev/disk/generic.h"
#include "dev/driver.h"
#include "fs/vnode.h"
#include "libk/error.h"
#include <fs/core.h>

int ramfs_init();
int ramfs_exit();

static int ramfs_read(vnode_t* node, void* buffer, size_t size, uintptr_t offset) {


  return 0;
}

static int ramfs_write(vnode_t* node, void* buffer, size_t size, uintptr_t offset) {
  kernel_panic("TODO: implement ramfs_write");
  return 0;
}

/*
 * We get passed the addressspace of the ramdisk through the partitioned_disk_dev_t
 * which holds the boundaries for our disk
 */
vnode_t* mount_ramfs(fs_type_t* type, const char* mountpoint, partitioned_disk_dev_t* device) {

  /* Since our 'lbas' are only one byte, we can obtain a size in bytes here =D */
  const generic_disk_dev_t* parent = device->m_parent;

  if (!parent)
    return nullptr;

  const size_t partition_size = device->m_partition_data.m_end_lba - device->m_partition_data.m_start_lba;
  vnode_t* node = create_generic_vnode("cramfs", VN_MOUNT | VN_FS);

  node->m_size = partition_size;
  node->m_data = (void*)device->m_partition_data.m_start_lba;

  node->f_read = ramfs_read;
  node->f_write = ramfs_write;

  return nullptr;
}

aniva_driver_t ramfs = {
  .m_name = "cramfs",
  .m_type = DT_FS,
  .f_init = ramfs_init,
  .f_exit = ramfs_exit,
};
EXPORT_DRIVER(ramfs);

fs_type_t cramfs = {
  .m_name = "cramfs",
  .f_mount = mount_ramfs,
  .m_flags = FST_REQ_DRIVER
};

int ramfs_init() {
  println("Initialized ramfs");

  int ret = 0;
  ErrorOrPtr result = register_filesystem(&cramfs);

  if (result.m_status == ANIVA_FAIL) {
    kernel_panic("Failed to register filesystem");
    ret = -1;
  }

  return ret;
}

int ramfs_exit() {

  return 0;
}
