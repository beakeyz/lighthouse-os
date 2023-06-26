#ifndef __ANIVA_DIRECTORY_VOBJ__
#define __ANIVA_DIRECTORY_VOBJ__

#include <libk/stddef.h>

struct vobj;
struct vdir_attr;

/*
 * Virtual directory
 *
 * Represents a fs-independent directory in a filesystem which acts as a namespace
 * for files to be ordered for easier access
 */
typedef struct vdir {
  
  struct vdir* m_parent_dir;
  struct vobj* m_parent_obj;
  struct vdir_attr* m_attr;

  const char* m_name;

  void* m_fs_data;

} vdir_t;

#define VDIR_FLAG_ROOT          (0x00000001)
#define VDIR_FLAG_SYSTEM        (0x00000002)
#define VDIR_FLAG_HIDDEN        (0x00000004)
#define VDIR_FLAG_TMP           (0x00000008)

typedef struct vdir_attr {
  uint32_t m_child_count;
  uint32_t m_flags;
  size_t m_total_size_under;
} vdir_attr_t;

#endif // !__ANIVA_DIRECTORY_VOBJ__
