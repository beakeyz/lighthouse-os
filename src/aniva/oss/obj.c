#include "obj.h"
#include "libk/flow/error.h"
#include "logging/log.h"
#include "mem/heap.h"
#include "oss/node.h"
#include "proc/proc.h"
#include "sched/scheduler.h"
#include "sync/atomic_ptr.h"
#include "sync/mutex.h"
#include <libk/string.h>
#include <proc/env.h>

static const enum HANDLE_TYPE ___oss_obj_type_to_handle_type_map[] = {
    [OSS_OBJ_TYPE_EMPTY] = HNDL_TYPE_NONE,
    [OSS_OBJ_TYPE_FILE] = HNDL_TYPE_FILE,
    [OSS_OBJ_TYPE_DIR] = HNDL_TYPE_DIR,
    [OSS_OBJ_TYPE_DEVICE] = HNDL_TYPE_DEVICE,
    [OSS_OBJ_TYPE_DRIVER] = HNDL_TYPE_DRIVER,
    [OSS_OBJ_TYPE_VAR] = HNDL_TYPE_SYSVAR,
    [OSS_OBJ_TYPE_PROC] = HNDL_TYPE_PROC,
    [OSS_OBJ_TYPE_PIPE] = HNDL_TYPE_UPI_PIPE,
};

HANDLE_TYPE oss_obj_type_to_handle_type(enum OSS_OBJ_TYPE type)
{
    /*
     * Check that we haven't missed any types
     * A user process should be able to hold handles to any oss object, so
     * every oss object type should have a associated handle type
     */
    ASSERT_MSG(sizeof(___oss_obj_type_to_handle_type_map) == (NR_OSS_OBJ_TYPES * sizeof(enum OSS_OBJ_TYPE)), "Size mismatch! Every oss object type should have a handle type!");

    /* Index into the oss-to-handle type map */
    return ___oss_obj_type_to_handle_type_map[type];
}

static void init_oss_obj_priv(oss_obj_t* obj, uint32_t priv_lvl)
{
    uint8_t obj_priv_lvl;
    proc_t* create_proc;

    create_proc = get_current_proc();

    /* Preemptive set to the biggest level */
    obj_priv_lvl = PRIV_LVL_ADMIN;

    /* If the caller specifies a desired privilege level, set it to that */
    if (priv_lvl < PRIV_LVL_NONE)
        obj_priv_lvl = priv_lvl;
    else if (create_proc)
        obj_priv_lvl = create_proc->m_env->priv_level;

    obj->access_priv_lvl = obj_priv_lvl;
    obj->read_priv_lvl = obj_priv_lvl;
    obj->write_priv_lvl = obj_priv_lvl;
}

/*!
 * @brief: Default object creation funciton
 */
oss_obj_t* create_oss_obj(const char* name)
{
    return create_oss_obj_ex(name, PRIV_LVL_NONE);
}

/*!
 * @brief: Extended object creation function
 *
 * It's the job of the object generator to ensure that the privilege level is correct. The
 * security responsibility lays there. This means for any userspace oss_obj access bugs, we first
 * need to look there
 */
oss_obj_t* create_oss_obj_ex(const char* name, uint32_t priv_lvl)
{
    oss_obj_t* ret;

    if (!name)
        return nullptr;

    ret = kmalloc(sizeof(*ret));

    if (!ret)
        return nullptr;

    memset(ret, 0, sizeof(*ret));

    ret->type = OSS_OBJ_TYPE_EMPTY;
    ret->name = strdup(name);
    ret->parent = nullptr;
    ret->lock = create_mutex(NULL);

    init_oss_obj_priv(ret, priv_lvl);

    init_atomic_ptr(&ret->refc, 1);
    return ret;
}

void destroy_oss_obj(oss_obj_t* obj)
{
    void* priv = obj->priv;
    oss_node_entry_t* entry = NULL;

    /* If we are still attached, might as well remove ourselves */
    if (obj->parent)
        oss_node_remove_entry(obj->parent, obj->name, &entry);

    /* Also destroy it's entry xD */
    if (entry)
        destroy_oss_node_entry(entry);

    /* Cache the private attachment */
    obj->priv = nullptr;

    if (obj->ops.f_destory_priv)
        obj->ops.f_destory_priv(priv);

    destroy_mutex(obj->lock);

    kfree((void*)obj->name);
    kfree(obj);
}

int oss_obj_set_priv_levels(oss_obj_t* obj, u32 level)
{
    if (!obj)
        return -KERR_INVAL;

    init_oss_obj_priv(obj, level);

    return 0;
}

const char* oss_obj_get_fullpath(oss_obj_t* obj)
{
    char* ret;
    char* suffix;
    size_t c_strlen;
    oss_node_t* c_parent;

    c_parent = obj->parent;

    if (!c_parent)
        return obj->name;

    suffix = strdup(obj->name);

    do {
        /* <name 1> / <name 2> <Nullbyte> */
        c_strlen = strlen(c_parent->name) + 1 + strlen(suffix) + 1;
        ret = kmalloc(c_strlen);
        memset(ret, 0, c_strlen);

        sfmt(ret, "%s/%s", c_parent->name, suffix);

        /* Free the current suffix */
        kfree(suffix);

        /* Set the suffix to the next 'item' */
        suffix = ret;

        /* Cycle parents */
        c_parent = c_parent->parent;
    } while (c_parent && c_parent->parent);

    // KLOG_DBG("oss_obj_get_fullpath: %s\n", ret);

    return ret;
}

void oss_obj_ref(oss_obj_t* obj)
{
    uint64_t val;

    mutex_lock(obj->lock);

    val = atomic_ptr_read(&obj->refc);
    atomic_ptr_write(&obj->refc, val + 1);

    mutex_unlock(obj->lock);
}

void oss_obj_unref(oss_obj_t* obj)
{
    uint64_t val;

    mutex_lock(obj->lock);

    val = atomic_ptr_read(&obj->refc) - 1;

    /* No need to write if  */
    if (val <= 0)
        goto destroy_obj;

    atomic_ptr_write(&obj->refc, val);

    mutex_unlock(obj->lock);
    return;

destroy_obj:
    mutex_unlock(obj->lock);
    destroy_oss_obj(obj);
}

/*!
 * @brief: Close a base object
 *
 * Called when any oss object is closed, regardless of underlying type. When the object
 * is not referenced anymore (TODO) it is automatically disposed of
 *
 * In the case of devices, drivers and such more permanent objects, they need to be present on
 * the oss tree regardless of if they are referenced or not. To combat their preemptive destruction,
 * we initialize them with a reference_count of one, to indicate they are 'referenced' by their
 * respective subsystem. When we want to destroy for example a device that has an oss_object attached
 * to it, the subsystem asks oss to dispose of the object, but if the object has a referencecount of > 1
 * the entire destruction opperation must fail, since persistent oss residents are essential to the
 * function of the system.
 */
int oss_obj_close(oss_obj_t* obj)
{
    if (!obj)
        return -1;

    oss_obj_unref(obj);
    return 0;
}

/*!
 * @brief: Rename an object
 */
int oss_obj_rename(oss_obj_t* obj, const char* newname)
{
    int error;
    oss_node_t* parent;
    oss_node_entry_t* entry;

    /* We simply can't have objects without a name lol */
    if (!obj->name)
        return -KERR_INVAL;

    mutex_lock(obj->lock);

    /* Object is not yet attached, no need to do weird things */
    if (!obj->parent) {
        kfree((void*)obj->name);

        obj->name = strdup(newname);

        mutex_unlock(obj->lock);
        return 0;
    }

    /* Cache the parent */
    parent = obj->parent;

    /* Remove this object */
    error = oss_node_remove_entry(parent, obj->name, &entry);

    if (error)
        goto unlock_and_exit;

    /* Kill the entry wrapper */
    destroy_oss_node_entry(entry);

    /* Kill the current name */
    kfree((void*)obj->name);

    /* Set the new name */
    obj->name = strdup(newname);

    /* Add the object again */
    error = oss_node_add_obj(parent, obj);

unlock_and_exit:
    mutex_unlock(obj->lock);
    return error;
}

/*!
 * @brief: Walk the parent chain of an object to find the root node
 */
struct oss_node* oss_obj_get_root_parent(oss_obj_t* obj)
{
    oss_node_t* c_node;

    c_node = obj->parent;

    while (c_node) {

        /* If there is no parent on an attached object, we have reached the root */
        if (!c_node->parent)
            return c_node;

        c_node = c_node->parent;
    }

    /* This should never happen lmao */
    return nullptr;
}

void oss_obj_register_child(oss_obj_t* obj, void* child, enum OSS_OBJ_TYPE type, FuncPtr destroy_fn)
{
    if (!obj)
        return;

    mutex_lock(obj->lock);

    switch (type) {
    case OSS_OBJ_TYPE_EMPTY:
        if (!obj->priv)
            goto exit_and_unlock;

        if (obj->ops.f_destory_priv)
            obj->ops.f_destory_priv(obj->priv);

        obj->ops.f_destory_priv = nullptr;
        obj->priv = nullptr;
        obj->type = type;
        break;
    default:
        obj->priv = child;
        obj->type = type;
        obj->ops.f_destory_priv = destroy_fn;
        break;
    }

exit_and_unlock:
    mutex_unlock(obj->lock);
}
