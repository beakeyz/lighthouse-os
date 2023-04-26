#ifndef __ANIVA_VNODE__
#define __ANIVA_VNODE__

#include "fs/file.h"
#include "libk/linkedlist.h"
#include <sync/mutex.h>
#include <libk/stddef.h>

struct vnode;
struct virtual_namespace;

/* TODO: */
enum VNODE_TYPES {
  DIR,
  MOUNTPOINT,
  FILE,
  // More
};

/* TODO: migrate ops to these structs */
struct generic_vnode_ops {
};

struct vnode_dir_ops {
  int (*f_create)(struct vnode* dir);
  int (*f_remove)(struct vnode* dir);
  int (*f_make)(struct vnode* dir);
  int (*f_rename)(struct vnode* dir, const char* new_name);
  /* TODO: ;-; */
};

typedef struct vnode {
  mutex_t* m_lock;

  /* pointer to this nodes 'device' in posix terms */
  void* m_dev;

  uint32_t m_flags;
  uint32_t m_access_count; /* Atomic? */
  uint32_t m_event_count;

  /* Gets assigned when the vnode is mounted somewhere in the VFS */
  int m_id;

  /* Context specific index of this vnode */
  uintptr_t m_inum;

  size_t m_size;

  const char* m_name;

  struct virtual_namespace* m_ns;

  void* m_data;

  /* Write data to this node (really should fail if the node isn't taken) */
  int (*f_write) (struct vnode*, void* buffer, size_t size, uintptr_t offset);

  /* Read data from this node (really should fail if the node isn't taken) */
  int (*f_read) (struct vnode*, void* buffer, size_t size, uintptr_t offset);

  /* Send some data to this node and have whatever is connected do something for you */
  int (*f_msg) (struct vnode*, driver_control_code_t code, void* buffer, size_t size);

  /* Should return bytestreams of their respective files */
  void* (*f_open_stream) (struct vnode*);
  void* (*f_close_stream) (struct vnode*);

  /* Take this node to do operations on it */
  int (*f_take) (struct vnode*, uint32_t flags);

  /* Release this node so others can do operations on it */
  int (*f_release) (struct vnode*);

  /* Grab named data associated with this node */
  struct vnode* (*f_find) (struct vnode*, char*);


  /* TODO: other vnode operations */

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

#define VN_ROOT     (0x000001) /* Is this node the root of something? */
#define VN_SYS      (0x000002) /* Is this node owned by the system? */
#define VN_MOUNT    (0x000004) /* Is this node a mountpoint for something? */
#define VN_FROZEN   (0x000008) /* Is this node frozen by the system? */
#define VN_CACHED   (0x000010) /* Does this node have cached data somewhere? */
#define VN_LINK     (0x000011) /* Does this node point to something else? */
#define VN_FS       (0x000012) /* Is this node a filesystem? */
#define VN_DIR      (0x000014) /* Is this node a directory? */
#define VN_TAKEN    (0x000018) /* Has someone taken this node? */

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
bool vn_seems_mounted(vnode_t);
bool vn_is_link(vnode_t);

/* 
 * Makes sure write opperations (or anything that changes 
 * the vnode) are either canceled or queued 
 */
void vn_freeze(vnode_t*);
void vn_unfreeze(vnode_t*);

#endif // !__ANIVA_VNODE__
