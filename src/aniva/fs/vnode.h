#ifndef __ANIVA_VNODE__
#define __ANIVA_VNODE__

#include "libk/linkedlist.h"
#include <sync/mutex.h>
#include <libk/stddef.h>

struct virtual_namespace;

typedef struct vnode {
  mutex_t* m_lock;

  void* m_dev;

  uint32_t m_flags;
  uint32_t m_access_count; /* Atomic? */
  uint32_t m_event_count;

  /* Gets assigned when the vnode is mounted somewhere in the VFS */
  int m_id;

  size_t m_size;

  const char* m_name;

  struct virtual_namespace* m_ns;

  void* m_data;

  int (*f_write) (struct vnode*, void* buffer, size_t size, uintptr_t offset);
  int (*f_read) (struct vnode*, void* buffer, size_t size, uintptr_t offset);
  int (*f_msg) (struct vnode*, driver_control_code_t code, void* buffer, size_t size);
  /* Should return bytestreams of their respective files */
  void* (*f_open_stream) (struct vnode*);
  void* (*f_close_stream) (struct vnode*);
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

#define VN_ROOT     (0x000001)
#define VN_SYS      (0x000002)
#define VN_MOUNT    (0x000004)
#define VN_FROZEN   (0x000008)
#define VN_CACHED   (0x000010)
#define VN_LINK     (0x000011)
#define VN_FS       (0x000012)
#define VN_DIR      (0x000014)

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
