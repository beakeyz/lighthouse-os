#include "vfs.h"
#include "dev/debug/serial.h"
#include "dev/driver.h"
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

ErrorOrPtr vfs_mount(const char* path, vnode_t* node) {

  if (!node) {
    return Error();
  }

  // If it ends at a namespace, all good, just attacht the node there
  vnamespace_t* namespace = hive_get(s_vfs.m_namespaces, path);

  if (!namespace) {
    return Error();
  }

  TRY(res, hive_add_entry(namespace->m_vnodes, node, node->m_name));

  node->m_ns = namespace;
  node->m_id = namespace->m_vnodes->m_total_entry_count + 1;
  node->m_flags |= VN_MOUNT;
  node->m_data = 0;
  node->m_size = 0;

  return res;
}

ErrorOrPtr vfs_mount_driver(const char* path, struct aniva_driver* driver) {

  vnode_t* current = vfs_resolve(path);

  if (current)
    return Error();

  vnode_t* node = create_fs_driver(driver);

  if (!node)
    return Error();

  println("Mounting...");
  println(path);
  TRY(mount_result, vfs_mount(path, node));

  return mount_result;
}

/*
 * Just unmount whatever we find at the end of this path
 */
ErrorOrPtr vfs_unmount(const char* path) {
  return Error();
}

/* TODO: support relative paths */
/* TODO: more robust resolving */
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
  
  vnamespace_t* namespace;

  char* new_namespace_id;
  size_t path_length = strlen(path);
  uintptr_t idx = path_length - 1;

  // Find the last part of the path
  while (idx && path[idx] && path[idx] != VFS_PATH_SEPERATOR) {
    idx--;
  }

  // We where able to reach the start of the path without finding
  // a seperator, we must want to attach a root.
  if (!idx) {
    namespace = create_vnamespace((char*)path, nullptr);

    return vfs_attach_root_namespace(namespace);
  }

  // The last part is the id of the new namespace
  new_namespace_id = (char*)&path[idx+1];

  // Store the rest of the path
  char path_copy[path_length];
  memcpy(path_copy, path, path_length);

  path_copy[idx] = '\0';

  // println(path_copy);
  // println(new_namespace_id);

  // Find the parent namespace, using the lhs of the path
  vnamespace_t* parent = hive_get(s_vfs.m_namespaces, path_copy);

  if (!parent) {
    return Error();
  }

  // Create a new namespace, using the rhs of the path
  namespace = create_vnamespace(new_namespace_id, parent);

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

void init_vfs(void) {

  // Init caches
  init_vcache();

  s_vfs.m_namespaces = create_hive("vfs_ns");
  s_vfs.m_lock = create_mutex(0);
  s_vfs.m_id = "system_vfs";

  // Init vfs namespaces
  init_vns();

}
