#include "group.h"
#include "dev/device.h"
#include "libk/flow/error.h"
#include "mem/heap.h"
#include "oss/core.h"
#include "oss/node.h"
#include "oss/obj.h"
#include <libk/string.h>

static dgroup_t* _create_dgroup()
{
  dgroup_t* ret;

  ret = kmalloc(sizeof(*ret));

  if (!ret)
    return nullptr;

  memset(ret, 0, sizeof(*ret));
  return ret;
}

static void _destroy_dgroup(dgroup_t* group)
{
  /* This is a dangerous call, since all downstream objects just get nuked */
  if (group->node)
    destroy_oss_node(group->node);
  kfree(group);
}

/*!
 * @brief: Create a new group and register it
 *
 * @parent: If parent is NULL, we register to the device rootnode
 */
dgroup_t* register_dev_group(enum DGROUP_TYPE type, const char* name, uint32_t flags, struct oss_node* parent)
{
  dgroup_t* group;

  group = _create_dgroup();

  if (!group)
    return nullptr;

  /* Create the node that houses this group */
  group->node = create_oss_node(name, OSS_GROUP_NODE, NULL, NULL);

  if (!group->node)
    goto dealloc_and_fail;

  group->node->priv = group;
  group->name = group->node->name;
  group->type = type;
  group->flags = flags;

  /* Register the group on the OSS endpoint */
  device_node_add_group(parent, group);
  return group;

dealloc_and_fail:
  _destroy_dgroup(group);
  return nullptr;
}

int unregister_dev_group(dgroup_t* group)
{
  _destroy_dgroup(group);
  return 0;
}

dgroup_t* dev_group_get_parent(dgroup_t* group)
{
  oss_node_t* parent;

  if (!group || !group->node)
    return nullptr;
  
  parent = group->node->parent;

  if (!parent->priv || parent->type != OSS_GROUP_NODE)
    return nullptr;

  /*
  * NOTE: Don't use oss_node_unwrap, since we know that
  * ->priv is non-null
  */
  return parent->priv;
}

/*!
 * @brief: Get a dgroup with the name @name
 *
 * 
 */
int dev_group_get(const char* path, dgroup_t** out)
{
  int error;
  oss_node_t* node;
  
  if (!out || !path)
    return -1;

  error = oss_resolve_node(path, &node);

  if (error)
    return error;

  if (!node || node->type != OSS_GROUP_NODE || !node->priv)
    return -KERR_INVAL;

  /* Found the bastard */
  *out = node->priv;
  return 0;
}

/*!
 * @brief: Get a device relative from @group
 *
 * @group: The group that the device is attached to
 * @path: The relative path from @group
 */
int dev_group_get_device(dgroup_t* group, const char* path, struct device** dev)
{
  int error;
  oss_node_t* node;
  oss_obj_t* obj;

  if (!group || !path || !dev)
    return -1;

  node = group->node;

  if (!node)
    return -1;

  error = oss_resolve_obj_rel(node, path, &obj);

  if (error)
    return error;

  /* Invalid attachment =( */
  if (!obj || obj->type != OSS_OBJ_TYPE_DEVICE)
    return -2;

  *dev = oss_obj_unwrap(obj, device_t);
  return 0;
}

int dev_group_add_device(dgroup_t* group, struct device* dev)
{
  if (!group || !dev)
    return -1;

  return oss_node_add_obj(group->node, dev->obj);
}

int dev_group_remove_device(dgroup_t* group, struct device* dev)
{
  oss_node_entry_t* entry;

  if (oss_node_remove_entry(group->node, dev->obj->name, &entry) || !entry)
    return -1;

  destroy_oss_node_entry(entry);
  return 0;
}

/*!
 * @brief: Initialize the device groups subsystem
 *
 * Make sure we're able to create, destroy, register, ect. device groups without
 * issues
 *
 * Currently no Initialize needed for dgroup, keeping this here just in case though
 */
void init_dgroups()
{
  (void)0;
}
