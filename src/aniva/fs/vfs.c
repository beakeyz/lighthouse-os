#include "vfs.h"
#include "libk/hive.h"
#include "mem/heap.h"

vfs_t* create_vfs(generic_disk_dev_t* device) {
  vfs_t* vfs = kmalloc(sizeof(vfs_t));

  vfs->m_device = device;
  vfs->m_fs_name = "A";
  vfs->m_fs_tree = create_hive("vfs_root");
  
  return vfs;
}

void destroy_vfs(vfs_t* fs) {
  destroy_hive(fs->m_fs_tree);
  kfree(fs);
}
