#ifndef __ANIVA_VFS_VOBJ__
#define __ANIVA_VFS_VOBJ__

#include "libk/flow/error.h"
#include "libk/flow/reference.h"
#include "sync/mutex.h"
#include <libk/stddef.h>

/*
 * A vobj represents anything that can be retrieved from the vfs
 */

struct vobj;
struct vnode;
struct file;
struct device;

typedef enum VOBJ_TYPE {
  VOBJ_TYPE_EMPTY = 0,
  /* Generic files in the filesystem get this */
  VOBJ_TYPE_FILE,
  /* A directory can be extracted from a filesystem, in which case it gets a vobj */
  VOBJ_TYPE_DIR,
  /* When we obtain a device from the vfs */
  VOBJ_TYPE_DEVICE,
  /* When we obtain a driver from the vfs */
  VOBJ_TYPE_DRIVER,
  /* This boi links to another vobj */
  VOBJ_TYPE_LINK,
  /*
   * This boi acts as a lookuptable (A vnode can create these types when 
   * it can be flexible about vobj creation). This table can result in more
   * vobjects that are managed by the taken vnode. If the node gets released,
   * this object also get demolished.
   */
  VOBJ_TYPE_TABLE,
} VOBJ_TYPE_t;

typedef uintptr_t vobj_handle_t;

typedef struct vobj_ops {
  struct vobj* (*f_create)(struct vnode* parent, const char* path);
  void (*f_destroy)(struct vobj* obj);

  /* Try to link this object */
  ErrorOrPtr (*f_link)(struct vobj*, struct vobj*);

} vobj_ops_t;

typedef struct vobj {
  const char* m_path;
  uint32_t m_flags;
  VOBJ_TYPE_t m_type;

  flat_refc_t m_refc;

  struct vnode* m_parent;
  mutex_t* m_lock;

  vobj_ops_t* m_ops;
  void (*f_destory_child)(void* child_ptr);

  struct vdir* m_parent_dir;

  /*
   * This is the handle that we can use to find this vobj implicitly.
   */
  vobj_handle_t m_handle;

  /* Context specific index of this vnode */
  uintptr_t m_inum;

  /* A vobj can be anything from a file to a directory, so keep a generic pointer around */
  void* m_child;

  union {
    struct vobj* m_ref;
    struct vobj* m_external_links;
  };

  struct vobj* m_next;
} vobj_t;

#define VOBJ_IMMUTABLE  0x00000001 /* Can we change the data that this object holds? */
#define VOBJ_CONFIG     0x00000002 /* Does this object point to configuration? */
#define VOBJ_SYS        0x00000004 /* Is this object owned by the system? */
#define VOBJ_ETERNAL    0x00000008 /* Does this vobj ever get cleaned? */
#define VOBJ_MOVABLE    0x00000010 /* Can this object be moved to different vnodes? */
#define VOBJ_REF        0x00000020 /* Does this object reference another object? */

#define INVALID_OBJ_HANDLE (vobj_handle_t)0

/*
 * Allocate space for a vobject and attach it to the parent vnode
 */ 
vobj_t* create_generic_vobj(struct vnode* parent, const char* path);
void destroy_vobj(vobj_t* obj);

void vobj_ref(vobj_t* obj);
int vobj_unref(vobj_t* obj);

void vobj_register_child(vobj_t* obj, void* child, VOBJ_TYPE_t type, FuncPtr destroy_fn);

int vobj_close(vobj_t* obj);

ErrorOrPtr vobj_generate_handle(vobj_t* object);
bool vobj_verify_handle(vobj_t* object);

struct file* vobj_get_file(vobj_t* obj);
struct device* vobj_get_device(vobj_t* obj);

#endif // !__ANIVA_VFS_VOBJ__
