#include "vfs.h"
#include "dev/debug/serial.h"
#include "dev/driver.h"
#include "dev/kterm/kterm.h"
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
#include <string.h>

static vfs_t s_vfs;

/*
 * Check if a path starts with the absolute character sequence,
 * which means we will have to search from the absolute root
 */
static bool __vfs_path_is_absolute(const char* path)
{
  return false;
}

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

  vnode_t* current = vfs_resolve_node(path);

  if (current)
    return Error();

  vnode_t* node = create_fs_driver(driver);

  if (!node)
    return Error();

  println("Mounting...");
  println(path);
  TRY(mount_result, vfs_mount(path, node));

  return Success(mount_result);
}

ErrorOrPtr vfs_mount_fs_type(const char* path, const char* mountpoint, struct fs_type* fs, partitioned_disk_dev_t* device) {

  vnode_t* mountnode = nullptr;

  /* Execute fs-specific mount routine */
  if (fs->f_mount)
    mountnode = fs->f_mount(fs, mountpoint, device);

  if (!mountnode)
    return Error();

  /* Mount into our vfs */
  ErrorOrPtr mount_result = vfs_mount(path, mountnode);

  return mount_result;
}

ErrorOrPtr vfs_mount_fs(const char* path, const char* mountpoint, const char* fs_name, partitioned_disk_dev_t* device) {

  fs_type_t* type = get_fs_type(fs_name);

  if (!type)
    return Error();

  return vfs_mount_fs_type(path, mountpoint, type, device);
}

/*
 * Just unmount whatever we find at the end of this path
 */
ErrorOrPtr vfs_unmount(const char* path) {
  return Error();
}

/* TODO: support relative paths */
/* TODO: more robust resolving */
/*
 * TODO: reimplement in order to do selective scanning
 * (
 *  Which is just walking the namespace/vnode/vobj tree and doing opperations
 *  based on what we find
 * )
 */
vobj_t* vfs_resolve(const char* path) {

  vobj_t* ret;
  vnode_t* end_vnode;
  vnamespace_t* end_namespace;

  uintptr_t node_id_index;
  char* node_id;
  char* obj_id;

  uintptr_t i;
  size_t path_length;

  if (!path)
    return nullptr;

  path_length = strlen(path);

  char path_copy[path_length + 1];
  path_copy[path_length] = NULL;

  /* Copy over path to buffer */
  memcpy(path_copy, path, path_length);

  i = 0;

  while (path_copy[i]) {

    if (path_copy[i] == VFS_PATH_SEPERATOR) {
      /* Reset byte */
      path_copy[i] = NULL;

      vnamespace_t* buffer = hive_get(s_vfs.m_namespaces, path_copy);

      /* Set back to the seperator early */
      path_copy[i] = VFS_PATH_SEPERATOR;

      /* If this is null we have reached the end of the namespace chain */
      if (!buffer)
        break;

      end_namespace = buffer;

      /* Buffer the node id just in case we can't find the next namespace */
      node_id_index = i + 1;
    }

    i++;
  }

  /* Could not find the namespace */
  if (!path_copy[i] || !end_namespace)
    return nullptr;

  /* Verify node id place */
  if (path_copy[node_id_index-1] != VFS_PATH_SEPERATOR)
    return nullptr;

  /* Reset the itterator index */
  i = node_id_index;

  /* Set the node id */
  node_id = &path_copy[node_id_index];

  /*
   * Shave off the next seperator 
   * NOTE: we should end this loop with the object id sitting at the end
   * of our index, since it comes right after the vnode path entry
   */
  while (path_copy[i]) {
    if (path_copy[i] == VFS_PATH_SEPERATOR) {
      path_copy[i] = NULL;
      break;
    }
    i++;
  }

  /* Find the node */
  end_vnode = hive_get(end_namespace->m_vnodes, node_id);

  if (!end_vnode)
    return nullptr;

  /* Place does not match */
  if (path_copy[i])
    return nullptr;

  /* Buffer the object id (path) which we have now reached */
  obj_id = &path_copy[i + 1];

  ret = end_vnode->m_ops->f_find(end_vnode, obj_id);

  return ret;
}

vnode_t* vfs_resolve_node(const char* path) {

  vnode_t* ret;
  vnamespace_t* end_namespace;

  uintptr_t node_id_index;
  char* node_id;
  char* obj_id;

  uintptr_t i;
  size_t path_length;

  if (!path)
    return nullptr;

  path_length = strlen(path);

  char path_copy[path_length + 1];
  path_copy[path_length] = NULL;

  /* Copy over path to buffer */
  memcpy(path_copy, path, path_length);

  i = 0;

  while (path_copy[i]) {

    if (path_copy[i] == VFS_PATH_SEPERATOR) {
      /* Reset byte */
      path_copy[i] = NULL;

      vnamespace_t* buffer = hive_get(s_vfs.m_namespaces, path_copy);

      /* Set back to the seperator early */
      path_copy[i] = VFS_PATH_SEPERATOR;

      /* If this is null we have reached the end of the namespace chain */
      if (!buffer)
        break;

      end_namespace = buffer;

      /* Buffer the node id just in case we can't find the next namespace */
      node_id_index = i + 1;
    }

    i++;
  }

  /* Could not find the namespace */
  if (!end_namespace)
    return nullptr;

  /* Verify node id */
  if (path_copy[node_id_index-1] != VFS_PATH_SEPERATOR)
    return nullptr;

  /* Reset the itterator index */
  i = node_id_index;

  /* Set the node id */
  node_id = &path_copy[node_id_index];

  /* Shave off the next seperator */
  while (path_copy[i]) {
    if (path_copy[i] == VFS_PATH_SEPERATOR) {
      path_copy[i] = NULL;
      break;
    }
    i++;
  }

  /* Find the node */
  ret = hive_get(end_namespace->m_vnodes, node_id);

  return ret;
}

static vnamespace_t* vfs_insert_namespace(const char* path, char* id, vnamespace_t* parent) {
  vnamespace_t* ns = create_vnamespace(id, parent);

  Must(hive_add_entry(s_vfs.m_namespaces, ns, path));

  return ns;
}

vnamespace_t* vfs_create_path(const char* path) {
  vnamespace_t* current_parent;
  vnamespace_t* result;
  uintptr_t previous_index = NULL;
  uintptr_t index = NULL;
  size_t path_size = strlen(path) + 1;
  uintptr_t path_end_index = strlen(path);

  char path_copy[path_size];

  memcpy(path_copy, path, path_size);

  /* Cut off any trailing slash */
  if (path[strlen(path)-1] == VFS_PATH_SEPERATOR) {
    path_end_index -= 1;
  }

  path_copy[path_end_index] = NULL;

  result = hive_get(s_vfs.m_namespaces, path_copy);

  if (result) {
    print("Found entry: ");
    println(result->m_id);
    return result;
  }

  while (true) {
    
    /* We want to know that our copy is valid and that the index is valid, before we check for seperator */
    while (path_copy[index] && index < path_size && path_copy[index] != VFS_PATH_SEPERATOR) {
      index++;
    }

    if (!path_copy[index] || index >= path_size) {
      /* We probably reached the end. Did we create all namespaces? */
      println("End");
      println(path_copy);
      println(&path_copy[previous_index]);
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
    path_copy[index] = VFS_PATH_SEPERATOR;
    index++;
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
  if (res_status.m_status == ANIVA_WARNING) {
    destroy_vnamespace(namespace);
  }

  return Success(res);
}

void init_vfs(void) {

  // Init caches
  init_vcache();

  s_vfs.m_namespaces = create_hive("vfs_ns");
  s_vfs.m_lock = create_mutex(0);

  char* root_id = VFS_ROOT_ID;

  s_vfs.m_id = kmalloc(strlen(root_id));
  memcpy(s_vfs.m_id, root_id, strlen(root_id));
  s_vfs.m_id[strlen(root_id)] = NULL;

  // Init vfs namespaces
  init_vns();

}
