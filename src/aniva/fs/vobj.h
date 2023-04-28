#ifndef __ANIVA_VFS_VOBJ__
#define __ANIVA_VFS_VOBJ__

#include "dev/handle.h"
#include "sync/mutex.h"
#include <libk/stddef.h>

/*
 * A vobj represents anything that can be retrieved from the vfs
 */

struct vobj;
struct vnode;

typedef struct vobj_ops {
  struct vobj* (*f_create)(struct vnode* parent, const char* path);
  void (*f_destroy)(struct vobj* obj);
  void (*f_destory_child)(handle_t child_ptr);
} vobj_ops_t;

typedef struct vobj {
  const char* m_path;
  uint32_t m_flags;

  struct vnode* m_parent;
  vobj_ops_t* m_ops;
  mutex_t* m_lock;

  /* Context specific index of this vnode */
  uintptr_t m_inum;

  /* A vobj can be anything from a file to a directory, so keep a generic pointer around */
  handle_t m_child;

  union {
    struct vobj* m_ref;
    struct vobj* m_external_links;
  };

  struct vobj* m_next;
} vobj_t;

#define VOBJ_IMMUTABLE  0x00000001 /* Can we change the data that this object holds? */
#define VOBJ_FILE       0x00000002 /* Is this object pointing to a generic file? */
#define VOBJ_CONFIG     0x00000004 /* Does this object point to configuration? */
#define VOBJ_SYS        0x00000008 /* Is this object owned by the system? */
#define VOBJ_ETERNAL    0x00000010 /* Does this vobj ever get cleaned? */
#define VOBJ_EMPTY      0x00000020 /* Is this vobject an empty shell? */
#define VOBJ_MOVABLE    0x00000040 /* Can this object be moved to different vnodes? */
#define VOBJ_REF        0x00000080 /* Does this object reference another object? */

vobj_t* create_generic_vobj(struct vnode* parent, const char* path);
void destroy_vobj(vobj_t* obj);

#endif // !__ANIVA_VFS_VOBJ__
