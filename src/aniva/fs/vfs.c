#include "vfs.h"
#include "dev/debug/serial.h"
#include "fs/cache.h"
#include "fs/namespace.h"
#include "fs/vnode.h"
#include "libk/error.h"
#include "libk/hive.h"
#include "libk/string.h"
#include "mem/heap.h"
#include "mem/kmem_manager.h"
#include "sync/mutex.h"
#include <libk/stddef.h>

static vfs_t s_vfs;

vnode_t* g_root_vnode;

void init_vfs(void) {

  // Init caches
  init_vcache();

  s_vfs.m_namespaces = create_hive("vfs_ns");
  s_vfs.m_id = "system_vfs";

  g_root_vnode = kmalloc(sizeof(vnode_t));
  memset(g_root_vnode, 0x00, sizeof(vnode_t));

  g_root_vnode->m_name = "/";
  g_root_vnode->m_lock = create_mutex(0);

  // Init vfs namespaces
  init_vns();

}

ErrorOrPtr vfs_mount(const char* path, vnode_t* node) {

  if (!node) {
    return Error();
  }

  // First parse the path

  // If it ends at a namespace, all good, just attacht the node there
  vnamespace_t* namespace = hive_get(s_vfs.m_namespaces, path);

  if (!namespace) {
    return Error();
  }

  /*
  const size_t node_end_path_len = (1 + strlen(node->m_name) + 1);
  const size_t full_path_len = (strlen(path) + 1 + strlen(node->m_name) + 1);

  char node_path_end[node_end_path_len];
  char full_path[full_path_len];

  memset(node_path_end, 0x00, node_end_path_len);
  memset(full_path, 0x00, full_path_len);

  concat("/", (char*)node->m_name, (char*)node_path_end);
  concat((char*)path, node_path_end, (char*)full_path);

  println("Pathes");
  println(node_path_end);
  println(full_path);
  */

  TRY(res, hive_add_entry(namespace->m_vnodes, node, node->m_name));

  return res;
}

/*
 * Just unmount whatever we find at the end of this path
 */
ErrorOrPtr vfs_unmount(const char* path) {
  return Error();
}

vnode_t* vfs_resolve(const char* path) {

  vnode_t* ret;

  uintptr_t i;
  const char* node_id;
  size_t path_length;

  path_length = strlen(path);
  i = path_length - 1;

  while (path[i] && path[i] != VFS_PATH_SEPERATOR) {
    i--;
  }

  node_id = &path[i+1];

  char path_copy[path_length];
  memcpy(path_copy, path, path_length);

  path_copy[i] = '\0';

  vnamespace_t* namespace = hive_get(s_vfs.m_namespaces, path_copy);

  if (!namespace)
    return nullptr;

  ret = hive_get(namespace->m_vnodes, node_id);

  if (ret)
    return ret;

  return nullptr;
}

ErrorOrPtr vfs_attach_namespace(const char* path) {

  if (!path)
    return Error();

  char* new_namespace_id;
  size_t path_length = strlen(path);
  uintptr_t idx = path_length - 1;

  // Find the last part of the path
  while (path[idx] && path[idx] != VFS_PATH_SEPERATOR) {
    idx--;
  }

  // The last part is the id of the new namespace
  new_namespace_id = (char*)&path[idx+1];

  // Store the rest of the path
  char path_copy[path_length];
  memcpy(path_copy, path, path_length);

  path_copy[idx] = '\0';

  println(path_copy);
  println(new_namespace_id);

  // Find the parent namespace, using the lhs of the path
  vnamespace_t* parent = hive_get(s_vfs.m_namespaces, path_copy);

  if (!parent) {
    return Error();
  }

  // Create a new namespace, using the rhs of the path
  vnamespace_t* namespace = create_vnamespace(new_namespace_id, parent);

  if (!namespace) {
    return Error();
  }

  TRY(res, hive_add_entry(s_vfs.m_namespaces, namespace, path));

  return Success(0);
}

ErrorOrPtr vfs_attach_root_namespace(vnamespace_t* namespace) {

  if (!namespace)
    return Error();

  TRY(res, hive_add_entry(s_vfs.m_namespaces, namespace, namespace->m_id));

  return res;
}
