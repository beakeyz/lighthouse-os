#include "vfs.h"
#include "fs/cache.h"
#include "fs/namespace.h"
#include "fs/vnode.h"
#include "libk/error.h"
#include "libk/hive.h"
#include "mem/heap.h"
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

  // First parse the path

  // If it ends at a namespace, all good, just attacht the node there

  // If it ends at a vnode, fuck you =)

  // If the path is invalid, idk. we can do whatever
  // either build a namespace chain to satisfy this request
  // or just say fuck you bro
  
  return Error();
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

ErrorOrPtr vfs_attach_namespace(const char* path, vnamespace_t* namespace) {

  if (!namespace)
    return Error();

  // Absolute entry
  if (!path) {
    TRY(res, hive_add_entry(s_vfs.m_namespaces, namespace, namespace->m_id));
  } else {
    TRY(res, hive_add_entry(s_vfs.m_namespaces, namespace, path));
  }

  return Success(0);
}
