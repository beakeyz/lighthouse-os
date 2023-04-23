#include "vfs.h"
#include "dev/debug/serial.h"
#include "dev/driver.h"
#include "fs/cache.h"
#include "fs/core.h"
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

  if (node->m_ns) {
    /* We don't yet allow multiple mounts, so TODO */
    return Error();
  }

  // If it ends at a namespace, all good. just attacht the node there
  vnamespace_t* namespace = vfs_create_path(path);

  if (!namespace) {
    return Error();
  }

  /* When we try to mount duplicates, we fail rn */
  Must(hive_add_entry(namespace->m_vnodes, node, node->m_name));

  vnode_t* test = hive_get(namespace->m_vnodes, node->m_name);

  node->m_ns = namespace;
  node->m_id = namespace->m_vnodes->m_total_entry_count + 1;
  node->m_flags |= VN_MOUNT;

  return Success(0);
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

ErrorOrPtr vfs_mount_fs_type(const char* mountpoint, struct fs_type* fs, partitioned_disk_dev_t* device) {

  vnode_t* mountnode = nullptr;

  println("Trying to mount fs type");

  if (fs->f_mount)
    mountnode = fs->f_mount(fs, mountpoint, device);

  if (!mountnode)
    return Error();

  println("Trying to really mount fs type");

  ErrorOrPtr mount_result = vfs_mount(mountpoint, mountnode);

  return mount_result;
}

ErrorOrPtr vfs_mount_fs(const char* mountpoint, const char* fs_name, partitioned_disk_dev_t* device) {

  fs_type_t* type = get_fs_type(fs_name);

  if (!type)
    return Error();

  return vfs_mount_fs_type(mountpoint, type, device);
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

  println(node_id);
  return hive_get(namespace->m_vnodes, node_id);
}

static vnamespace_t* vfs_insert_namespace(const char* path, char* id, vnamespace_t* parent) {
  vnamespace_t* ns = create_vnamespace(id, parent);

  Must(hive_add_entry(s_vfs.m_namespaces, ns, path));

  return ns;
}

vnamespace_t* vfs_create_path(const char* path) {
  vnamespace_t* current_parent;
  vnamespace_t* result = hive_get(s_vfs.m_namespaces, path);
  uintptr_t previous_index = NULL;
  uintptr_t index = NULL;
  size_t path_size = strlen(path) + 1;

  if (result) {
    print("Found entry: ");
    println(result->m_id);
    return result;
  }

  char path_copy[path_size];

  memset(path_copy, 0x00, path_size);
  memcpy((void*)path_copy, path, path_size);

  while (true) {
    
    /* We want to know that our copy is valid and that the index is valid, before we check for seperator */
    while (path_copy[index] && index < path_size && path_copy[index] != VFS_PATH_SEPERATOR) {
      index++;
    }

    if (!path_copy[index] || index >= path_size) {
      /* We probably reached the end. Did we create all namespaces? */
      return vfs_insert_namespace(path_copy, &path_copy[previous_index], current_parent);
    }

    /* Null-terminate */
    path_copy[index] = 0x00;

    // result = vfs_ensure_attached_namespace(path_copy);
    if ((result = hive_get(s_vfs.m_namespaces, path_copy)) == nullptr) {
      /* Create TODO should have parent */
      current_parent = result = vfs_insert_namespace(path_copy, &path_copy[previous_index], current_parent);
    }

    /* If attaching the namespace failed fsr, we can die */
    if (!result) {
      break;
    }

    /* Since we know that there was a seperator at this spot, just place it back */
    path_copy[index++] = VFS_PATH_SEPERATOR;
    previous_index = index;
  }

  return result;
}

vnamespace_t* vfs_ensure_attached_namespace(const char* path) {

  if (!path)
    return nullptr;
  
  vnamespace_t* namespace = hive_get(s_vfs.m_namespaces, path);

  /* Only now it's a valid namespace */
  if (namespace) {
    if (namespace->m_parent) {
      return namespace;
    }

    /* TODO: walk the path to resolve the parents? */
    kernel_panic("Namespace had no parent!");
  }

  char* new_namespace_id;
  size_t path_length = strlen(path);
  uintptr_t idx = 0;

  // Find the last part of the path
  while (idx && path[idx] && path[idx] != VFS_PATH_SEPERATOR) {
    idx--;
  }

  // We where able to reach the start of the path without finding
  // a seperator, we must want to attach a root.
  if (!idx) {
    namespace = create_vnamespace((char*)path, nullptr);

    if (namespace && vfs_attach_root_namespace(namespace).m_status == ANIVA_FAIL) {
      destroy_vnamespace(namespace);
      return nullptr;
    }

    return namespace;
  }

  // The last part is the id of the new namespace
  new_namespace_id = (char*)&path[idx+1];

  // Store the rest of the path
  char path_copy[path_length];
  memcpy(path_copy, path, path_length);

  path_copy[idx] = '\0';

  // Find the parent namespace, using the lhs of the path
  vnamespace_t* parent = hive_get(s_vfs.m_namespaces, path_copy);

  if (!parent) {
    return nullptr;
  }

  // Create a new namespace, using the rhs of the path
  namespace = create_vnamespace(new_namespace_id, parent);

  if (!namespace) {
    return nullptr;
  }

  print("Parent path: ");
  println(path_copy);
  print("Parent: ");
  println(parent->m_id);

  ErrorOrPtr res =  hive_add_entry(s_vfs.m_namespaces, namespace, path);

  if (res.m_status != ANIVA_SUCCESS) {
    destroy_vnamespace(namespace);
    return nullptr;
  }

  return namespace;
}

ErrorOrPtr vfs_attach_root_namespace(vnamespace_t* namespace) {

  if (!namespace)
    return Error();

  TRY(res, hive_add_entry(s_vfs.m_namespaces, namespace, namespace->m_id));

  /* Warning means that the namespace already exists in this case */
  /* TODO: make error shit more clear lmao */
  if (res.m_status == ANIVA_WARNING) {
    destroy_vnamespace(namespace);
  }

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
