#ifndef __ANIVA_DIRECTORY_VOBJ__
#define __ANIVA_DIRECTORY_VOBJ__

#include <libk/stddef.h>

struct vobj;
struct vdir;
struct vdir_ops;
struct vdir_attr;

/*
 * Virtual directory
 *
 * Represents a fs-independent directory in a filesystem which acts as a namespace
 * for files to be ordered for easier access
 */
typedef struct vdir {

  /* This name is simply the name of this directory only. This does not include any slashes or path seperators */
  const char* m_name;
  uint32_t m_name_len;
  
  struct vdir* m_parent_dir;
  struct vdir_attr* m_attr;
  struct vdir_ops* m_ops;

  /* Linked list of the cached objects in this vdir */
  struct vobj* m_objects;

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

typedef struct vdir_ops {
  int (*f_destroy)(struct vdir*);
  int (*f_put_obj)(struct vdir*, struct vobj*);
} vdir_ops_t;

#endif // !__ANIVA_DIRECTORY_VOBJ__
