#ifndef __ANIVA_VNODE__
#define __ANIVA_VNODE__

#include "fs/file.h"
#include "fs/vobj.h"
#include "libk/flow/error.h"
#include "libk/data/linkedlist.h"
#include <sync/mutex.h>
#include <libk/stddef.h>

struct vobj;
struct vnode;
struct virtual_namespace;
struct partitioned_disk_dev;

/* TODO: */
enum VNODE_TYPES {
  DIR,
  MOUNTPOINT,
  FILE,
  // More
};

/* TODO: migrate ops to these structs */
struct generic_vnode_ops {
  /* Send some data to this node and have whatever is connected do something for you */
  int (*f_msg) (struct vnode*, driver_control_code_t code, void* buffer, size_t size);

  /* Force a sync between diffed buffers and the device */
  int (*f_force_sync) (struct vnode*);
  int (*f_force_sync_once) (struct vnode*, struct vobj*);

  int (*f_makedir)(struct vnode*, void* dir_entry, void* dir_attr, uint32_t flags);
  int (*f_rmdir)(struct vnode*, void* dir_entry, uint32_t flags);

  /* Grab named data associated with this node */
  struct vobj* (*f_open) (struct vnode*, char*);

  /* Close a vobject that has been opened by this vnode */
  int (*f_close)(struct vnode*, struct vobj*); 
};

struct vnode_dir_ops {
  int (*f_create)(struct vnode* dir);
  int (*f_remove)(struct vnode* dir);
  int (*f_make)(struct vnode* dir);
  int (*f_rename)(struct vnode* dir, const char* new_name);
};

#define VN_PERTDEV(node) ((struct partitioned_disk_dev*)node->m_dev)

typedef struct vnode {
  mutex_t* m_lock;
  mutex_t* m_vobj_lock;

  /* pointer to this nodes 'device' in posix terms */
  void* m_dev;

  uint32_t m_flags;
  uint32_t m_access_count; /* Atomic? */
  uint32_t m_event_count;

  /* Gets assigned when the vnode is mounted somewhere in the VFS */
  int m_id;

  size_t m_size;

  const char* m_name;

  struct virtual_namespace* m_ns;

  uint8_t* m_data;

  struct partitioned_disk_dev* m_device;
  struct generic_vnode_ops* m_ops; 
  struct vnode_dir_ops* m_dir_ops;

  /*
   * Objects can be anything from files to directories to simply kobjects
   * and they should probably be stored in a hashtable here lol
   */
  struct vobj* m_objects;
  size_t m_object_count;

  /* 
   * A node is either linked, or linking. We can't have a link referencing 
   * another link in aniva.
   */
  union {
    /*
     * If this node links to another node, this points to that node, 
     * and all opperations may be forwarded to that node.
     */
    struct vnode* m_ref;
    list_t* m_links;
  };
} vnode_t;

#define VN_ROOT     (0x00000001) /* Is this node the root of something? */
#define VN_SYS      (0x00000002) /* Is this node owned by the system? */
#define VN_MOUNT    (0x00000004) /* Is this node a mountpoint for something? */
#define VN_FROZEN   (0x00000008) /* Is this node frozen by the system? */
#define VN_CACHED   (0x00000010) /* Does this node have cached data somewhere? */
#define VN_LINK     (0x00000020) /* Does this node point to something else? */
#define VN_FS       (0x00000040) /* Is this node a filesystem? */
#define VN_DIR      (0x00000080) /* Is this node a directory? */
#define VN_TAKEN    (0x00000100) /* Has someone taken this node? */

/* When a flexible node is taken anyway, behaviour should not change */
#define VN_FLEXIBLE (0x00000200) /* Flexible nodes allow opperations while they are not taken */

vnode_t* create_generic_vnode(const char* name, uint32_t flags);

/*
 * Due to the nature of vnodes, we can only destroy them if they are 
 * generic. If they have drivers, filesystems, ect. attached, those 
 * need to be cleaned up first...
 *
 * vnodes also can't be mounted when they are destroyed. We mostly leave
 * this for the caller, but we fail nontheless if the node *seems* mounted
 * (aka. the fields of the node suggest that it is)
 */
ErrorOrPtr destroy_generic_vnode(vnode_t*);

bool vn_is_socket(vnode_t);
bool vn_is_system(vnode_t);

bool vn_is_available(vnode_t* node);

bool vn_seems_mounted(vnode_t);
bool vn_is_link(vnode_t);

/* 
 * Makes sure write opperations (or anything that changes 
 * the vnode) are either canceled or queued 
 */
void vn_freeze(vnode_t*);
void vn_unfreeze(vnode_t*);

/*
 * Taking a vnode means you take custody of all the vobjects that
 * result out of this vnode. When you release this node, all of these 
 * objects will be synced, destroyed and/or saved.
 */
int vn_take(vnode_t* node, uint32_t flags);
int vn_release(vnode_t* node);

ErrorOrPtr vn_attach_object(vnode_t* node, struct vobj* obj);
ErrorOrPtr vn_detach_object(vnode_t* node, struct vobj* obj);
ErrorOrPtr vn_detach_object_path(vnode_t* node, const char* path);

struct vobj* vn_get_object(vnode_t* node, const char* path);
struct vobj* vn_get_object_idx(vnode_t* node, uintptr_t idx);
ErrorOrPtr vn_obj_get_index(vnode_t* node, struct vobj*);

bool vn_has_object(vnode_t* node, const char* path);

ErrorOrPtr vn_link(vnode_t* node, vnode_t* link);
ErrorOrPtr vn_unlink(vnode_t* node, vnode_t* link);

vnode_t* vn_get_link(vnode_t* node, const char* link_name);

struct vobj* vn_open(vnode_t* node, char* name);
int vn_close(vnode_t* node, struct vobj* obj);

#endif // !__ANIVA_VNODE__
