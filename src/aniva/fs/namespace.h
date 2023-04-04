#ifndef __ANIVA_FS_NAMESPACE__
#define __ANIVA_FS_NAMESPACE__

#include "libk/error.h"
#include "libk/hive.h"
#include "libk/linkedlist.h"
#include <libk/stddef.h>

/*
 * Every vnode that is inside the VFS gets asigned a namespace.
 * we traverse the VFS through these namespaces (?)
 */

struct vnode;

typedef struct virtual_namespace {
  /* how many vnamespaces are in between this namespace and the root */
  uintptr_t m_distance_to_root;

  /* Id that specifies this namespace */
  char* m_id;

  /* Simple string that describes the type of vnodes this namespace facilitates */
  char* m_desc;

  /* Some flags that help us manage capabilities of this namespace */
  uint32_t m_flags;

  /* Should be a hash table */
  hive_t* m_vnodes;

  struct virtual_namespace* m_parent;
} vnamespace_t;

#define VNS_SYSTEM (0x00000001) /* Owned by the kernel */
#define VNS_FROZEN (0x00000002) /* Writes are discarded */

void init_vns();

vnamespace_t* create_vnamespace(char* id, vnamespace_t* parent);

/*
 * Binds a node to a namespace
 * if the node already has a namespace, it fails
 */
ErrorOrPtr vns_assign_vns(struct vnode* node, vnamespace_t* namespace);

/*
 * Binds a node to a namespace
 * if the node does not already has a namespace, it fails
 */
ErrorOrPtr vns_reassign_vns(struct vnode* node, vnamespace_t* namespace);

#endif // !__ANIVA_FS_NAMESPACE__