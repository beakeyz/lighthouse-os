#include "vdir.h"
#include "fs/vnode.h"
#include "fs/vobj.h"
#include "libk/flow/error.h"
#include "logging/log.h"
#include "mem/heap.h"
#include <mem/zalloc.h>

zone_allocator_t* __vdir_attr_allocator;

static vdir_ops_t __generic_vdir_ops = {
  0,
};

static void __link_vdir(vdir_t* parent, vdir_t* dir)
{
  dir->m_next_sibling = parent->m_subdirs;
  parent->m_subdirs = dir;
}

static void __try_prepend_parents_name(vdir_t* parent, vdir_t* dir)
{
  if (!parent || !dir->m_name)
    return;

  size_t new_len = parent->m_name_len + 1 + dir->m_name_len;
  char buffer[new_len + 1];

  memset(buffer, 0, new_len + 1);

  memcpy(buffer, parent->m_name, parent->m_name_len);
  buffer[parent->m_name_len] = '/';
  memcpy(&buffer[parent->m_name_len + 1], dir->m_name, dir->m_name_len);

  kfree((void*)dir->m_name);

  dir->m_name = strdup(buffer);
  dir->m_name_len = new_len;
}

vdir_t* create_vdir(vnode_t* node, vdir_t* parent, const char* name)
{
  vdir_t* ret;

  if (!node || !name)
    return nullptr;

  ret = zalloc_fixed(node->m_vdir_allocator);

  memset(ret, 0, sizeof(*ret));

  ret->m_name_len = strlen(name);
  ret->m_name = strdup(name);

  ASSERT_MSG(ret->m_name, "Got a vdir without a name!");

  __try_prepend_parents_name(parent, ret);

  ret->m_attr = zalloc_fixed(__vdir_attr_allocator);
  ret->m_ops = &__generic_vdir_ops;

  if (!parent)
    return ret;

  ret->m_parent_dir = parent;

  __link_vdir(parent, ret);

  return ret;
}

void destroy_vdir(vdir_t* dir)
{
  vdir_attr_t* attributes;

  if (dir->m_ops && dir->m_ops->f_destroy)
    dir->m_ops->f_destroy(dir);

  ASSERT_MSG(dir->m_attr && dir->m_attr->m_parent_node, "Tried to destroy a vdir without a parent!");

  kfree((void*)dir->m_name);

  attributes = dir->m_attr;

  zfree_fixed(attributes->m_parent_node->m_vdir_allocator, dir);
  zfree_fixed(__vdir_attr_allocator, attributes);
}

ErrorOrPtr destroy_vdirs(vdir_t* root, bool destroy_objs)
{
  /* Loop over every subdir and sibling */
  /* Destroy any vobj if destroy_objs is true */
  /* Destroy the vdir */
  ErrorOrPtr result;
  vdir_t* current_subdir = nullptr,
        * next_subdir = nullptr;
  vdir_t* current_sibling = nullptr,
        * next_sibling = nullptr;
  vobj_t* current_vobj = nullptr,
        * next_vobj = nullptr;
  
  if (!root)
    return Error();

  current_sibling = root;
  current_subdir = current_sibling->m_subdirs;

  /* Skip this part if we don't need to kill vobjs */
  if (!destroy_objs)
    goto destroy_recursive;

  current_vobj = root->m_objects;

  while (current_vobj) {
    next_vobj = current_vobj->m_next;

    vn_close(current_vobj->m_parent, current_vobj);

    current_vobj = next_vobj;
  }

destroy_recursive:
  /*
   * Loop over the siblings in the root directory
   */
  while (current_sibling) {
    next_sibling = current_sibling->m_next_sibling;

    while (current_subdir) {
      next_subdir = current_subdir->m_next_sibling;

      result = destroy_vdirs(current_subdir, destroy_objs);

      if (IsError(result))
        return result;

      current_subdir = next_subdir;
    }

    /* Break early so we can kill this vdir */
    if (!next_sibling)
      break;

    destroy_vdir(current_sibling);

    current_sibling = next_sibling;
  }

  return Success(0);
}

vobj_t* vdir_find_vobj(vdir_t* dir, const char* path)
{
  vobj_t* ret;

  if (!path || !dir)
    return nullptr;

  for (ret = dir->m_objects; ret; ret = ret->m_next) {
    if (strcmp(ret->m_path, path) == 0)
      break;
  }

  if (ret)
    return ret;

  if (dir->m_ops && dir->m_ops->f_find_obj)
    ret =  dir->m_ops->f_find_obj(dir, path);

  return ret;
}

int vdir_put_vobj(vdir_t* dir, struct vobj* obj)
{
  if (!dir || !obj)
    return -1;

  obj->m_parent_dir = dir;

  if (dir->m_ops && dir->m_ops->f_put_obj)
    return dir->m_ops->f_put_obj(dir, obj);

  obj->m_next = dir->m_objects;
  dir->m_objects = obj;

  return 0;
}

void init_vdir(void)
{
  __vdir_attr_allocator = create_zone_allocator(4 * Kib, sizeof(vdir_attr_t), NULL);
}
