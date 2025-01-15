#include "group.h"
#include "dev/device.h"
#include "lightos/api/objects.h"
#include "mem/heap.h"
#include "oss/core.h"
#include "oss/object.h"
#include <libk/string.h>

static dgroup_t* __create_dgroup()
{
    dgroup_t* ret;

    ret = kmalloc(sizeof(*ret));

    if (!ret)
        return nullptr;

    memset(ret, 0, sizeof(*ret));

    return ret;
}

static void __destroy_dgroup(dgroup_t* group)
{
    kfree(group);
}

static int __oss_destroy_dgroup(oss_object_t* object)
{
    dgroup_t* group;

    if (!object || object->type != OT_DGROUP || !object->private)
        return -EINVAL;

    group = object->private;

    __destroy_dgroup(group);
    return 0;
}

static oss_object_ops_t dgroup_oss_ops = {
    .f_Destroy = __oss_destroy_dgroup,
};

/*!
 * @brief: Create a new group and register it
 *
 * @parent: If parent is NULL, we register to the device rootnode
 */
dgroup_t* register_dev_group(enum DGROUP_TYPE type, const char* name, uint32_t flags, oss_object_t* parent)
{
    dgroup_t* group;

    group = __create_dgroup();

    if (!group)
        return nullptr;

    /* Create the node that houses this group */
    group->object = create_oss_object(name, OF_NO_DISCON, OT_DGROUP, &dgroup_oss_ops, group);

    if (!group->object)
        goto dealloc_and_fail;

    group->name = group->object->key;
    group->type = type;
    group->flags = flags;
    group->parent = parent;

    /* Register the group on the OSS endpoint */
    device_node_add_group(parent, group);
    return group;

dealloc_and_fail:
    __destroy_dgroup(group);
    return nullptr;
}

int unregister_dev_group(dgroup_t* group)
{
    /* First disconnect the group from its parent */
    oss_object_disconnect(group->parent, group->object);

    /* Then close our own object as to throw away our own reference */
    oss_object_close(group->object);

    return 0;
}

dgroup_t* dev_group_get_parent(dgroup_t* group)
{
    oss_object_t* parent = group->parent;

    if (parent->type != OT_DGROUP)
        return nullptr;

    /*
     * NOTE: Don't use oss_node_unwrap, since we know that
     * ->priv is non-null
     */
    return parent->private;
}

/*!
 * @brief: Get a dgroup with the name @name
 *
 *
 */
int dev_group_get(const char* path, dgroup_t** out)
{
    int error;
    oss_object_t* obj;

    if (!path || !out)
        return -EINVAL;

    error = oss_open_object(path, &obj);

    if (error)
        return error;

    /* Check if this object is okay lol */
    if (obj->type != OT_DGROUP || !obj->private) {
        oss_object_close(obj);
        return -EINVAL;
    }

    *out = obj->private;

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
    oss_object_t* obj;

    if (!group || !path || !dev)
        return -EINVAL;

    /* Try a relative open call */
    error = oss_open_object_from(path, group->object, &obj);

    if (error)
        return error;

    /* Check type */
    if (obj->type != OT_DEVICE || !obj->private) {
        oss_object_close(obj);
        return -EINVAL;
    }

    /* Export */
    *dev = obj->private;

    return 0;
}

int dev_group_add_device(dgroup_t* group, struct device* dev)
{
    if (!group || !dev)
        return -1;

    return oss_object_connect(group->object, dev->obj);
}

int dev_group_remove_device(dgroup_t* group, struct device* dev)
{
    return oss_object_disconnect(group->object, dev->obj);
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
