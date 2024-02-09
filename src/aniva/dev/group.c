#include "group.h"
#include "dev/device.h"
#include "libk/data/hashmap.h"
#include "libk/flow/error.h"
#include "libk/string.h"
#include "oss/node.h"
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
  kernel_panic("TODO: _create_dgroup");
  return nullptr;
}

static void _destroy_dgroup(dgroup_t* group)
{
  kernel_panic("TODO: _destroy_dgroup");
}

int register_dev_group(enum DGROUP_TYPE type, const char* name, uint32_t flags)
{
  dgroup_t* group;

  if (_group_map->m_size >= _get_group_capacity())
    return -1;

  group = _create_dgroup();

  if (!group)
    return -1;

  group->name = strdup(name);
  group->type = type;
  group->flags = flags;
  /* Create the node that houses this group */
  group->node = create_oss_node(name, OSS_OBJ_STORE_NODE, NULL, NULL);
  group->node->priv = group;

  /* Register the group on the OSS endpoint */
  device_add_group(group);
  kernel_panic("TODO: register_dev_group");
  return 0;
}

int unregister_dev_group(dgroup_t* group)
{
  kernel_panic("TODO: unregister_dev_group");
  _destroy_dgroup(group);
  return 0;
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
