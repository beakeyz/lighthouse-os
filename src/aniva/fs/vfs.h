// TODO: move this to src/fs and use certain architecture-specific shit 
// depending on the arch

#ifndef __ANIVA_VFS__
#define __ANIVA_VFS__

#include <libk/hive.h>

#define ANIVA_MAX_VFS_COUNT 4

struct vfs;


typedef struct vfs {
  const char m_fs_name[32];
  
  hive_t* m_fs_tree;
} vfs_t;

vfs_t* create_vfs();
void destroy_vfs(vfs_t* fs);

// TODO: fill in templates. These functions are incomplete
ErrorOrPtr vfs_mount();
ErrorOrPtr vfs_unmount();



vfs_t g_vfs_list[ANIVA_MAX_VFS_COUNT];
#endif // !__ANIVA_VFS__
