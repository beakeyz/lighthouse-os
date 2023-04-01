// TODO: move this to src/fs and use certain architecture-specific shit 
// depending on the arch

#ifndef __ANIVA_VFS__
#define __ANIVA_VFS__

#include "dev/disk/generic.h"
#include <libk/hive.h>

struct vfs;

typedef struct vfs {
  char* m_fs_name;

  generic_disk_dev_t* m_device;
  
  hive_t* m_fs_tree;
} vfs_t;

vfs_t* create_vfs(generic_disk_dev_t* device);
void destroy_vfs(vfs_t* fs);

// TODO: fill in templates. These functions are incomplete
ErrorOrPtr vfs_mount();
ErrorOrPtr vfs_unmount();

#endif // !__ANIVA_VFS__
