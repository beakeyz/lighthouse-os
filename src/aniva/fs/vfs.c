#include "vfs.h"
#include "dev/core.h"
#include "dev/debug/serial.h"
#include "dev/driver.h"
#include "dev/manifest.h"
#include "fs/core.h"
#include "fs/vdir.h"
#include "fs/namespace.h"
#include "fs/vnode.h"
#include "fs/vobj.h"
#include "libk/flow/error.h"
#include "libk/data/hive.h"
#include "libk/string.h"
#include "logging/log.h"
#include "mem/heap.h"
#include "mem/kmem_manager.h"
#include "sync/mutex.h"
#include <libk/stddef.h>

static vfs_t s_vfs;

/*
 * Check if a path starts with the absolute character sequence,
 * which means we will have to search from the absolute root
 */
static bool __vfs_path_is_absolute(const char* path)
{
  return (path[0] == VFS_ABS_PATH_IDENTIFIER);
}

vnamespace_t* vfs_get_abs_root() 
{
  return hive_get(s_vfs.m_namespaces, VFS_ROOT_ID);
}

static ErrorOrPtr _vfs_mount(const char* path, vnode_t* node) 
{
  if (!node)
    return Error();

  if (node->m_ns)
    /* We don't yet allow multiple mounts, so TODO */
    return Error();

  if (!node->m_ops)
    return Error();

  /* These are the essensial opperations for a fs on our system */
  if (!node->m_ops->f_open || !node->m_ops->f_close || !node->m_ops->f_msg)
    return Error();

  // If it ends at a namespace, all good. just attacht the node there
  vnamespace_t* namespace = vfs_create_ns_path(path);

  if (!namespace)
    return Error();

  /* When we try to mount duplicates, we die rn */
  Must(hive_add_entry(namespace->m_vnodes, node, node->m_name));

  node->m_ns = namespace;
  node->m_id = namespace->m_vnodes->m_total_entry_count + 1;
  node->m_flags |= VN_MOUNT;

  return Success(0);
}

ErrorOrPtr vfs_mount_driver(const char* path, struct aniva_driver* driver) {

  dev_url_t url;
  dev_manifest_t* manifest;
  vnode_t* current = vfs_resolve_node(path);

  if (current)
    return Error();

  url = get_driver_url(driver);

  if (!url)
    return Error();
  
  manifest = get_driver(url);

  kfree((void*)url);

  if (!manifest)
    return Error();

  vnode_t* node = create_fs_driver(manifest);

  if (!node)
    return Error();

  TRY(mount_result, _vfs_mount(path, node));

  return Success(mount_result);
}

ErrorOrPtr vfs_mount_raw(const char* path, vnode_t* node)
{
  /* Prevent vnodes with a filesystem to be mounted at this path */
  if (!node || node->fs_data.m_type)
    return Error();

  return _vfs_mount(path, node);
}

ErrorOrPtr vfs_mount_fs_type(const char* path, const char* mountpoint, struct fs_type* fs, partitioned_disk_dev_t* device) 
{
  vnode_t* mountnode = nullptr;

  /* Execute fs-specific mount routine */
  if (fs->f_mount)
    mountnode = fs->f_mount(fs, mountpoint, device);

  if (!mountnode)
    return Error();

  /* Make sure that the node knows its filesystem type */
  mountnode->fs_data.m_type = fs;

  /* Mount into our vfs */
  return _vfs_mount(path, mountnode);
}

/*!
 * @brief: Mount a filesystem vnode on the specified @mountpoint
 *
 * TODO: Check if a particular device has already been mounted before somewhere else, 
 * and let this new mount simply point to the old mount, to prevent double mounting of 
 * the same filesystem
 *
 * @path: The namespace path to follow
 * @mountpoint: The 'name' that the vnode will have
 * @fs_name: The filesystem type
 * @device: The memory space manager that the filesystem will have
 */
ErrorOrPtr vfs_mount_fs(const char* path, const char* mountpoint, const char* fs_name, partitioned_disk_dev_t* device) {

  fs_type_t* type = get_fs_type(fs_name);

  if (!type)
    return Error();

  return vfs_mount_fs_type(path, mountpoint, type, device);
}

/*
 * Just unmount whatever we find at the end of this path
 *
 * NOTE: this path HAS to be absolute
 */
ErrorOrPtr vfs_unmount(const char* path) 
{
  int error;
  vnode_t* node;
  vnamespace_t* namespace;
  fs_type_t* fs_type;

  if (!path)
    return Error();

  node = vfs_resolve_node(path);

  if (!node || !node->m_ns)
    return Error();

  /* Take the node to give any pending processes time to finish their shit */
  error = vn_take(node, NULL);

  if (error)
    return Error();

  /* If this node has a filesystem, it needs to murder itself */
  fs_type = node->fs_data.m_type;
  error = 0;

  /* Notify the filesystem we want to unmount */
  if (fs_type && fs_type->f_unmount)
    error = fs_type->f_unmount(fs_type, node);

  if (error)
    return Error();

  /* Grab the namespace */
  namespace = node->m_ns;

  /* Remove the vnode from its namespace */
  error = vns_remove_vnode(namespace, node);

  if (error)
    return Error();

  /* Death */
  return destroy_generic_vnode(node);
}

vobj_t* vfs_resolve_relative(vnamespace_t* rel_ns, vnode_t* node, vdir_t* dir, const char* path)
{
  vobj_t* ret;
  uint32_t i;
  char* obj_id;
  size_t path_size = strlen(path);
  char buffer[path_size + 1];

  if (!path)
    return nullptr;

  if (!node && !rel_ns && !dir)
    return nullptr;

  memset(buffer, 0, path_size + 1);

  /* Shift the pointer forward to cut of the seperator */
  if (path[0] == VFS_PATH_SEPERATOR) {
    path_size--;

    memcpy(buffer, path + 1, path_size);
  } else {
    memcpy(buffer, path, path_size);
  }

  buffer[path_size] = NULL;

  if (!rel_ns && !dir && node)
    return vn_open(node, buffer); 

  if (!rel_ns)
    return nullptr;

  i = 0;

  /*
   * Shave off the next seperator 
   * NOTE: we should end this loop with the object id sitting at the end
   * of our index, since it comes right after the vnode path entry
   */
  while (buffer[i]) {
    if (buffer[i] == VFS_PATH_SEPERATOR) {
      buffer[i] = NULL;
      break;
    }
    i++;
  }

  /* Find the node */
  node = hive_get(rel_ns->m_vnodes, buffer);

  if (!node)
    return nullptr;

  /* Place does not match */
  if (buffer[i])
    return nullptr;

  /* Buffer the object id (path) which we have now reached */
  obj_id = &buffer[i + 1];

  /* No need to check if this is flexible, because resolve needs to be chill here */
  vn_take(node, NULL);

  ret = vn_open(node, obj_id);

  vn_release(node);

  return ret;
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

  if (!__vfs_path_is_absolute(path))
    return vfs_resolve_relative(vfs_get_abs_root(), nullptr, nullptr, path);

  path_length = strlen(path);
  node_id_index = NULL;
  end_namespace = NULL;

  char path_copy[path_length + 1];
  path_copy[path_length] = NULL;

  if (path[0] == VFS_PATH_SEPERATOR) {
    path_length--;

    /* Cut off the seperator at the start */
    memcpy(path_copy, path+1, path_length);
  } else {
    /* Copy over path to buffer */
    memcpy(path_copy, path, path_length);
  }


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

  /* No need to check if this is flexible, because resolve needs to be chill here */
  vn_take(end_vnode, NULL);

  ret = vn_open(end_vnode, obj_id);

  vn_release(end_vnode);

  return ret;
}

vnode_t* vfs_resolve_node(const char* path) {

  vnode_t* ret;
  vnamespace_t* end_namespace;

  uintptr_t node_id_index;
  char* node_id;

  uintptr_t i;
  size_t path_length;

  if (!path)
    return nullptr;

  /* Path to resolve a vnode must be absolute */
  if (!__vfs_path_is_absolute(path))
    return nullptr;

  end_namespace = nullptr;
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

int vfs_copy(const char* from, const char* to)
{
  kernel_panic("TODO: vfs_copy");
  return 0;
}
int vfs_remove(const char* path)
{
  kernel_panic("TODO: vfs_remove");
  return 0;
}

int vfs_create(const char* path, enum VNODE_CREATE_MODE mode)
{
  kernel_panic("TODO: vfs_create");
  return 0;
}

static vnamespace_t* vfs_insert_namespace(const char* path, char* id, vnamespace_t* parent) {
  vnamespace_t* ns = create_vnamespace(id, parent);

  Must(hive_add_entry(s_vfs.m_namespaces, ns, path));

  return ns;
}

vnamespace_t* vfs_create_ns_path(const char* path) 
{
  vnamespace_t* current_parent = nullptr;
  vnamespace_t* result = nullptr;
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
    return Warning();
  }

  return Success(res);
}

/*!
 * @brief Find the filesystem type of a certain vnode
 *
 * Nothing to add here...
 */
fs_type_t* vfs_get_fs_at(const char* path)
{
  vnode_t* node;

  if (!path)
    return nullptr;

  node = vfs_resolve_node(path);

  if (!node)
    return nullptr;

  return node->fs_data.m_type;
}

void init_vfs(void) {

  s_vfs.m_namespaces = create_hive("vfs_ns");
  s_vfs.m_lock = create_mutex(0);
  s_vfs.m_id = VFS_ROOT_ID; 

  // Init vfs namespaces
  init_vns();
}
