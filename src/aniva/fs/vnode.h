#ifndef __ANIVA_VNODE__
#define __ANIVA_VNODE__

#include <sync/mutex.h>
#include <libk/stddef.h>

struct virtual_namespace;

typedef struct vnode {
  mutex_t* m_lock;

  void* m_dev;

  uint32_t m_flags;
  uint32_t m_access_count;
  uint32_t m_event_count;

  /* Gets assigned when the vnode is mounted somewhere in the VFS */
  uintptr_t m_id;

  size_t m_size;

  const char* m_name;

  struct virtual_namespace* m_ns;

  void* m_data;

  void (*f_write) (struct vnode*, uintptr_t off, size_t size, void* buffer);
  void (*f_read) (struct vnode*, uintptr_t off, size_t size, void* buffer);
  void (*f_msg) (struct vnode*, driver_control_code_t code, packet_payload_t payload, packet_response_t** buffer);
  /* Should return bytestreams of their respective files */
  void* (*f_open_stream) (struct vnode*);
  void* (*f_close_stream) (struct vnode*);
  /* TODO: other vnode operations */

  struct vnode* m_parent;
  struct vnode* m_ref;
} vnode_t;

#define VN_ROOT     (0x000001)
#define VN_SYS      (0x000002)
#define VN_MOUNT    (0x000004)
#define VN_FROZEN   (0x000008)

bool vn_is_socket(vnode_t);
bool vn_is_system(vnode_t);

/* 
 * Makes sure write opperations (or anything that changes 
 * the vnode) are either canceled or queued 
 */
void vn_freeze(vnode_t*);
void vn_unfreeze(vnode_t*);

#endif // !__ANIVA_VNODE__
