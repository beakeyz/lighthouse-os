#include "namespace.h"
#include "fs/vfs.h"
#include "fs/vnode.h"
#include "libk/flow/error.h"
#include "libk/data/hive.h"
#include "libk/stddef.h"
#include "mem/heap.h"
#include "sync/mutex.h"

void init_vns() {

  // Install the system namespace
  vnamespace_t* root_namespace = create_vnamespace(ROOT_NAMESPACE_ID, nullptr);

  root_namespace->m_flags |= VNS_SYSTEM;
  root_namespace->m_desc = "sys";

  vfs_attach_root_namespace(root_namespace);
}

vnamespace_t* create_vnamespace(char* id, vnamespace_t* parent) {
  if (!id)
    return nullptr;

  vnamespace_t* ns = kmalloc(sizeof(vnamespace_t));

  if (!ns)
    return nullptr;

  memset(ns->m_id, 0, sizeof(ns->m_id));
  memcpy(ns->m_id, id, (strlen(id) >= sizeof(ns->m_id)) ? sizeof(ns->m_id) : strlen(id));

  ns->m_vnodes = create_hive(ns->m_id);
  ns->m_desc = nullptr;
  ns->m_flags = NULL;
  ns->m_parent = parent;
  ns->m_mutate_lock = create_mutex(0);

  ns->m_distance_to_root = 0;

  if (parent)
    ns->m_distance_to_root = parent->m_distance_to_root + 1;

  return ns;
}

void destroy_vnamespace(vnamespace_t* ns) 
{
  if (!ns)
    return;

  destroy_mutex(ns->m_mutate_lock);
  destroy_hive(ns->m_vnodes);
  kfree(ns);
}

ErrorOrPtr vns_assign_vnode(struct vnode* node, vnamespace_t* namespace) {

  if (!node)
    return Error();

  if (!namespace)
    return Error();

  if (node->m_ns)
    return Error();

  node->m_ns = namespace;

  TRY(res, hive_add_entry(namespace->m_vnodes, node, node->m_name));

  return Success(0);
}

ErrorOrPtr vns_reassign_vnode(struct vnode* node, vnamespace_t* namespace) {

  if (!node)
    return Error();

  if (!namespace)
    return Error();

  if (!node->m_ns)
    return Error();

  // Remove from the old namespace
  TRY(remove_res, hive_remove(node->m_ns->m_vnodes, node));

  // Update nodes namespace
  node->m_ns = namespace;

  // Add to new namespace
  TRY(add_res, hive_add_entry(namespace->m_vnodes, node, node->m_name));

  return Success(0);
}

vnode_t* vns_find_vnode(vnamespace_t* ns, const char* path) {

  vnode_t* ret = hive_get(ns->m_vnodes, path);

  if (!ret) {
    /* Look in the stashed vnodes perhaps */
  }

  return ret;
}

vnode_t* vns_try_remove_vnode(vnamespace_t* ns, const char* path) {

  vnode_t* ret = vns_find_vnode(ns, path);

  if (!ret)
    return nullptr;

  ErrorOrPtr result = hive_remove(ns->m_vnodes, ret);

  if (result.m_status == ANIVA_FAIL) {
    return nullptr;
  }

  return ret;
}

int vns_remove_vnode(vnamespace_t* ns, struct vnode* node)
{
  int error;
  ErrorOrPtr result;

  if (!ns || !node)
    return -1;

  if (node->m_ns != ns)
    return -2;

  if (!vn_is_available(node))
    return -3;

  error = vns_commit_mutate(ns);

  if (error)
    return -4;

  result = hive_remove(ns->m_vnodes, node);
  error = vns_uncommit_mutate(ns);

  if (error || IsError(result))
    return -5;

  /* Clear the nodes state fields */
  node->m_ns = nullptr;
  node->m_id = 0;
  node->m_flags &= ~VN_MOUNT;

  return 0;
}

int vns_commit_mutate(vnamespace_t* ns) 
{
  if (mutex_is_locked(ns->m_mutate_lock))
    return -1;

  mutex_lock(ns->m_mutate_lock);

  return 0;
}

int vns_uncommit_mutate(vnamespace_t* ns) 
{
  if (!mutex_is_locked(ns->m_mutate_lock))
    return -1;

  mutex_unlock(ns->m_mutate_lock);

  return 0;
}

bool vns_contains_vnode(vnamespace_t* ns, const char* path) {
  return (vns_find_vnode(ns, path) != nullptr);
}

bool vns_contains_obj(vnamespace_t* ns, struct vnode* obj) {
  return hive_contains(ns->m_vnodes, obj);
}

void vns_stash_vnode(vnamespace_t* ns, struct vnode* node) {
  kernel_panic("TODO: (vns_stash_vnode) implement vnode stashing");
}

void vns_activate_vnode(vnamespace_t* ns, struct vnode* node) {
  kernel_panic("TODO: (vns_activate_vnode) implement vnode stashing");
}

static void vns_mutate(vnamespace_t* namespace, vnamespace_t obj) {


  if (!namespace)
    return;

  /* TODO: when the old namespace still contains nodes, what do we want to do with them? */
  if (namespace->m_vnodes) {
    /* Right now we just simply yeet out all the nodes it previously had, so the caller must respect that */
    destroy_hive(namespace->m_vnodes);
  }
  
  memcpy(namespace, &obj, sizeof(vnamespace_t));
}

ErrorOrPtr vns_try_move(vnamespace_t** ns_p, const char* path) 
{
  int error;
  ErrorOrPtr result;
  vnamespace_t* ns;
  if (!ns_p)
    return Error();
 
  ns = *ns_p;

  if (!ns)
    return Error();

  error = vns_commit_mutate(ns);

  if (error)
    return Error();

  /* Remember the size of the seperator */
  size_t full_path_len = strlen(path) + 1 + strlen(ns->m_id);

  char full_path[full_path_len + 1];

  /* Move the namespace */
  vnamespace_t* dummy = vfs_create_path(full_path);

  if (!dummy)
    return Error();

  vns_mutate(dummy, *ns);

  /* Make sure the old namespace is dead */
  destroy_vnamespace(ns);

  /* Update the reference */
  *ns_p = dummy;

  /* If, at this point, we fail to uncommit; something has gone very wrong */
  error = vns_uncommit_mutate(*ns_p);

  if (error)
    return Warning();

  return result;
}

