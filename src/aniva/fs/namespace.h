#ifndef __ANIVA_FS_NAMESPACE__
#define __ANIVA_FS_NAMESPACE__

#include "libk/flow/error.h"
#include "libk/data/hive.h"
#include "libk/data/linkedlist.h"
#include "sync/mutex.h"
#include <libk/stddef.h>

/*
 * Every vnode that is inside the VFS gets asigned a namespace.
 * we traverse the VFS through these namespaces (?)
 */

struct vnode;

#define ROOT_NAMESPACE_ID ":"

/* TODO: work out how we can stash away vnodes */
typedef struct vnode_stash {
  
} vnode_stash_t;

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

  /* Locks the mutating functionality of a namespace */
  mutex_t* m_mutate_lock;

  struct virtual_namespace* m_parent;
} vnamespace_t;

#define VNS_SYSTEM    (0x00000001) /* Owned by the kernel */
#define VNS_FROZEN    (0x00000002) /* Writes are discarded */
#define VNS_HAS_STASH (0x00000004) /* Does this namespace has stashed nodes */

void init_vns();

/*
 * Create a namespace instance
 */
vnamespace_t* create_vnamespace(char* id, vnamespace_t* parent);

void destroy_vnamespace(vnamespace_t* ns);

/*
 * Binds a node to a namespace
 * if the node already has a namespace, it fails
 */
ErrorOrPtr vns_assign_vnode(struct vnode* node, vnamespace_t* namespace);

/*
 * Locking for namespaces. Namespaces get locked when we do vnode resolutions,
 * in order to ensure the namespace does not get yeeted right when we try to 
 * find it. When a vnode is grabbed by something (a process, the kernel, ect.)
 * we simply send it vnamespace events to signal it has moved location (when this 
 * is necessery) ((TODO))
 */
ErrorOrPtr vns_commit_mutate(vnamespace_t* ns);
ErrorOrPtr vns_uncommit_mutate(vnamespace_t* ns);

/*
 * Binds a node to a namespace
 * if the node does not already has a namespace, it fails
 */
ErrorOrPtr vns_reassign_vnode(struct vnode* node, vnamespace_t* namespace);

struct vnode* vns_find_vnode(vnamespace_t* ns, const char* path);

struct vnode* vns_try_remove_vnode(vnamespace_t* ns, const char* path);

bool vns_contains_vnode(vnamespace_t* ns, const char* path);

bool vns_contains_obj(vnamespace_t* ns, struct vnode* obj);

/* 
 * Cache a vnode to speed up searches (we might want to do 
 * this if a vnode isnt used much
*/
void vns_stash_vnode(vnamespace_t* ns, struct vnode* node);

/*
 * Reverse action to the above func
 */
void vns_activate_vnode(vnamespace_t* ns, struct vnode* node);

void vns_replace_vnode(vnamespace_t* ns, const char* path, struct vnode* node);

static ALWAYS_INLINE const char* vns_stash_prefix() {
  return "@stashed";
}

/*
 * NOTE: The new path excludes the id of the new namespace
 */
ErrorOrPtr vns_try_move(vnamespace_t** ns, const char* path);

#endif // !__ANIVA_FS_NAMESPACE__
