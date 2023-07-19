#ifndef __ANIVA_DIRECTORY_VOBJ__
#define __ANIVA_DIRECTORY_VOBJ__

#include "libk/flow/error.h"
#include <libk/stddef.h>

struct vobj;
struct vdir;
struct vnode;
struct vdir_ops;
struct vdir_attr;

/*
 * Virtual directory
 *
 * Represents a fs-independent directory in a filesystem which acts as a namespace
 * for files to be ordered for easier access
 */
typedef struct vdir {

  /*
   * This name is simply a combination of all the earlier paths 
   * ( Root/  -> Root/Data/ )
   * ( Root  -> Root/Data )
   * ( /Root  -> /Root/Data )
   */
  const char* m_name;
  uint32_t m_name_len;
  
  struct vdir* m_parent_dir;

  struct vdir_attr* m_attr;
  struct vdir_ops* m_ops;

  /* Subdirs under this vdir */
  struct vdir* m_subdirs;

  /* Next sibling directory in this subdirectory */
  struct vdir* m_next_sibling;

  /* Linked list of the cached objects in this vdir */
  struct vobj* m_objects;

} vdir_t;

#define VDIR_FLAG_ROOT          (0x00000001)
#define VDIR_FLAG_SYSTEM        (0x00000002)
#define VDIR_FLAG_HIDDEN        (0x00000004)
#define VDIR_FLAG_TMP           (0x00000008)

void init_vdir(void);

vdir_t* create_vdir(struct vnode* node, vdir_t* parent, const char* name);
void destroy_vdir(vdir_t* dir);

ErrorOrPtr destroy_vdirs(vdir_t* root, bool destroy_objs);

struct vobj* vdir_find_vobj(vdir_t* dir, const char* path);
int vdir_put_vobj(vdir_t* dir, struct vobj* obj);

typedef struct vdir_attr {
  struct vnode* m_parent_node;
  uint32_t m_flags;
} vdir_attr_t;

typedef struct vdir_ops {
  int (*f_destroy)(struct vdir*);
  int (*f_put_obj)(struct vdir*, struct vobj*);
  struct vobj* (*f_find_obj)(struct vdir*, const char*);
} vdir_ops_t;

#endif // !__ANIVA_DIRECTORY_VOBJ__
