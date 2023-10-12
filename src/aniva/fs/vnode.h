#ifndef __ANIVA_VNODE__
#define __ANIVA_VNODE__

#include "dev/disk/generic.h"
#include "fs/core.h"
#include "fs/file.h"
#include "fs/vobj.h"
#include "libk/flow/error.h"
#include "libk/data/linkedlist.h"
#include "mem/zalloc.h"
#include <sync/mutex.h>
#include <libk/stddef.h>

struct vobj;
struct vnode;
struct virtual_namespace;

enum VNODE_TYPES {
  VNODE_NONE = 0,
  VNODE_MOUNTPOINT, /* Every mountpoint has a partitioned disk device */
  VNODE_DRIVER,     /* Every driver has a drv object attached */
};

/* TODO: clean this up and move functions to their supposed structs */
struct generic_vnode_ops {
  /* Send some data to this node and have whatever is connected do something for you */
  int (*f_msg) (struct vnode*, driver_control_code_t code, void* buffer, size_t size);

  /* Force a sync between diffed buffers and the device */
  int (*f_force_sync) (struct vnode*);
  int (*f_force_sync_once) (struct vnode*, struct vobj*);

  int (*f_makedir)(struct vnode*, void* dir_entry, void* dir_attr, uint32_t flags);
  int (*f_rmdir)(struct vnode*, void* dir_entry, uint32_t flags);

  /* Grab or create named data associated with this node */
  struct vobj* (*f_open) (struct vnode*, char*);

  /* Close a vobject that has been opened by this vnode */
  int (*f_close)(struct vnode*, struct vobj*); 

  void (*destroy_fs_specific_info)(void*);
  int (*reload_sb)(struct vnode*);
  void (*remount_fs)(struct vnode*);
};

/* TODO: migrate */
struct vnode_dir_ops {
  int (*f_create)(struct vnode* dir);
  int (*f_remove)(struct vnode* dir);
  int (*f_make)(struct vnode* dir);
  int (*f_rename)(struct vnode* dir, const char* new_name);
};

typedef struct vnode {
  mutex_t* m_lock;
  mutex_t* m_vobj_lock;

  uint32_t m_flags;
  uint32_t m_access_count; /* Atomic? */
  uint32_t m_event_count;

  /* Gets assigned when the vnode is mounted somewhere in the VFS */
  int m_id;

  const char* m_name;

  enum VNODE_TYPES m_type;

  union {
    partitioned_disk_dev_t* m_device;
    /* pointer to this nodes 'device' in posix terms */
    void* m_drv;
  };

  struct virtual_namespace* m_ns;
  struct generic_vnode_ops* m_ops; 
  struct vnode_dir_ops* m_dir_ops;

  zone_allocator_t* m_vdir_allocator;

  /*
   * Objects can be anything from files to directories to simply kobjects
   * and they should probably be stored in a hashtable here lol
   */
  struct vdir* m_root_vdir;
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
  /* This sub-struct contains the fields that tell us stuff about the filesystem */
  struct {
    fs_type_t* m_type;

    uint32_t m_blocksize;
    uint32_t m_flags;

    /* FIXME: are these fields supposed to to be 64-bit? */
    uint32_t m_dirty_blocks;
    uint32_t m_faulty_blocks;
    size_t m_free_blocks;
    size_t m_total_blocks;

    uintptr_t m_first_usable_block;
    uintptr_t m_max_filesize;

    void* m_fs_specific_info;

  } fs_data;
} vnode_t;

#define VN_FS_DATA(node) (node)->fs_data

#define VN_ROOT     (0x00000001) /* Is this node the root of something? */
#define VN_SYS      (0x00000002) /* Is this node owned by the system? */
#define VN_MOUNT    (0x00000004) /* Is this node a mountpoint for something? */
#define VN_FROZEN   (0x00000008) /* Is this node frozen by the system? */
#define VN_CACHED   (0x00000010) /* Does this node have cached data somewhere? */
#define VN_LINK     (0x00000020) /* Does this node point to something else? */
#define VN_FS       (0x00000040) /* Is this node a filesystem? */
#define VN_DIR      (0x00000080) /* Is this node a directory? */
#define VN_TAKEN    (0x00000100) /* Has someone taken this node? */
#define VN_NO_WAIT  (0x00000200) /* Never wait for a node to be released */

/* When a flexible node is taken anyway, behaviour should not change */
#define VN_FLEXIBLE (0x00000200) /* Flexible nodes allow opperations while they are not taken */

vnode_t* create_generic_vnode(const char* name, uint32_t flags);

static inline void vnode_set_type(vnode_t* node, enum VNODE_TYPES type)
{
  node->m_type = type;
}

static inline bool vnode_has_driver(vnode_t* node) 
{
  return (node->m_type == VNODE_DRIVER && node->m_drv);
}

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

/* Attach relative to the root vdir */
ErrorOrPtr vn_attach_object(vnode_t* node, struct vobj* obj);

/* Attach relative from the given vdir */
ErrorOrPtr vn_attach_object_rel(vnode_t* node, struct vdir* dir, struct vobj* obj);

ErrorOrPtr vn_detach_object(vnode_t* node, struct vobj* obj);
ErrorOrPtr vn_detach_object_path(vnode_t* node, const char* path);

struct vobj* vn_get_object_rel(vnode_t* node, struct vdir* dir, const char* path);
struct vobj* vn_get_object(vnode_t* node, const char* path);

bool vn_has_object(vnode_t* node, const char* path);

ErrorOrPtr vn_link(vnode_t* node, vnode_t* link);
ErrorOrPtr vn_unlink(vnode_t* node, vnode_t* link);

vnode_t* vn_get_link(vnode_t* node, const char* link_name);

struct vobj* vn_open(vnode_t* node, char* name);
int vn_close(vnode_t* node, struct vobj* obj);

#endif // !__ANIVA_VNODE__
