#include "namespace.h"
#include "fs/vfs.h"
#include "fs/vnode.h"
#include "libk/error.h"
#include "libk/hive.h"
#include "libk/stddef.h"
#include "mem/heap.h"

void init_vns() {
  ASSERT_MSG(g_root_vnode != nullptr, "Can't initialize vnamespaces without knowing the root vnode");

  vnamespace_t* root_namespace = create_vnamespace("root", nullptr);

  vfs_attach_namespace(nullptr, root_namespace);

  vns_assign_vns(g_root_vnode, root_namespace);
  
}

vnamespace_t* create_vnamespace(char* id, vnamespace_t* parent) {
  if (!id)
    return nullptr;

  vnamespace_t* ns = kmalloc(sizeof(vnamespace_t));

  if (!ns)
    return nullptr;

  ns->m_children = create_hive(id);
  ns->m_vnodes = create_hive(id);
  ns->m_id = id;
  ns->m_parent = parent;

  ns->m_distance_to_root = 0;

  if (parent)
    ns->m_distance_to_root = parent->m_distance_to_root + 1;

  return ns;
}

ErrorOrPtr vns_assign_vns(struct vnode* node, vnamespace_t* namespace) {

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

ErrorOrPtr vns_reassign_vns(struct vnode* node, vnamespace_t* namespace) {

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
