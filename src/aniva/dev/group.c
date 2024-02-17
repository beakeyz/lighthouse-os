#include "group.h"
#include "dev/device.h"
#include "libk/data/hashmap.h"
#include "libk/flow/error.h"
#include "libk/string.h"
#include "mem/heap.h"
#include "oss/core.h"
#include "oss/node.h"
#include "oss/obj.h"
#include "sync/mutex.h"

static hashmap_t* _group_map;
static mutex_t* _group_lock;

static inline size_t _get_group_capacity()
{
  if (!_group_map)
    return NULL;

  return _group_map->m_max_entries;
}

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

  if (_group_map->m_size >= _get_group_capacity())
    return nullptr;

  group = _create_dgroup();

  if (!group)
    return nullptr;

  group->name = strdup(name);
  group->type = type;
  group->flags = flags;
  /* Create the node that houses this group */
  group->node = create_oss_node(name, OSS_GROUP_NODE, NULL, NULL);
  group->node->priv = group;

  mutex_lock(_group_lock);

  /* Add to our general group map */
  hashmap_put(_group_map, (hashmap_key_t)name, group);

  mutex_unlock(_group_lock);

  /* Register the group on the OSS endpoint */
  device_node_add_group(parent, group);
  return group;
}

int unregister_dev_group(dgroup_t* group)
{
  dgroup_t* check;

  if (!group)
    return -1;

  mutex_lock(_group_lock);

  /* Remove from our general group map */
  check = hashmap_remove(_group_map, (hashmap_key_t)group->name);

  mutex_unlock(_group_lock);

  ASSERT_MSG(check == group, "Encountered a mismatch in unregister_dev_group");

  /* This would fucking suck lmao */
  if (check != group)
    return -1;

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
int dev_group_get(const char* name, dgroup_t** out)
{
  if (!out || !name)
    return -1;

  mutex_lock(_group_lock);

  *out = hashmap_get(_group_map, (hashmap_key_t)name);

  mutex_unlock(_group_lock);
  return (*out) == nullptr ? -1 : 0;
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
 */
void init_dgroups()
{
  _group_map = create_hashmap(128, HASHMAP_FLAG_SK);
  _group_lock = create_mutex(NULL);
}
