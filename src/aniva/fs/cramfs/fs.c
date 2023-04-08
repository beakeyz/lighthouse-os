#include "dev/core.h"
#include "dev/debug/serial.h"
#include "dev/driver.h"
#include "fs/vnode.h"
#include "libk/error.h"
#include <fs/core.h>

int ramfs_init();
int ramfs_exit();

vnode_t* mount_ramfs(fs_type_t* type, const char* mountpoint, partitioned_disk_dev_t* device) {

  vnode_t* node = create_generic_vnode("cramfs", VN_MOUNT);

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
