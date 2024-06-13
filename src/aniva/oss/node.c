#include "node.h"
#include "fs/dir.h"
#include "libk/data/hashmap.h"
#include "libk/flow/error.h"
#include "mem/heap.h"
#include "obj.h"
#include "sync/mutex.h"
#include <libk/string.h>
#include <mem/zalloc/zalloc.h>

/*
 * Core code for the oss nodes
 *
 * This code should really only supply basic routines to manipulate the node layout
 * The core handles any weird stuff with object generators
 */

#define SOFT_OSS_NODE_OBJ_MAX 0x1000

static zone_allocator_t* _node_allocator;
static zone_allocator_t* _entry_allocator;

void init_oss_nodes()
{
    /* Initialize the caches */
    _node_allocator = create_zone_allocator(16 * Kib, sizeof(oss_node_t), NULL);
    _entry_allocator = create_zone_allocator(16 * Kib, sizeof(oss_node_entry_t), NULL);
}

/*!
 * @brief: Allocate memory for a oss node
 *
 * NOTE: It's OK for @ops and @parent to be NULL
 */
oss_node_t* create_oss_node(const char* name, enum OSS_NODE_TYPE type, struct oss_node_ops* ops, struct oss_node* parent)
{
    oss_node_t* ret;

    if (!name)
        return nullptr;

    ret = zalloc_fixed(_node_allocator);

    if (!ret)
        return nullptr;

    memset(ret, 0, sizeof(*ret));

    ret->name = strdup(name);
    ret->type = type;
    ret->parent = parent;
    ret->ops = ops;

    ret->lock = create_mutex(NULL);
    /* TODO: Allow the hashmap to be resized */
    ret->obj_map = create_hashmap(SOFT_OSS_NODE_OBJ_MAX, NULL);

    return ret;
}

/*!
 * @brief: Clean up oss node memory
 */
void destroy_oss_node(oss_node_t* node)
{
    oss_node_entry_t* entry;

    if (!node)
        return;

    entry = nullptr;

    /* Remove ourselves if we're still attached */
    if (node->parent)
        oss_node_remove_entry(node->parent, node->name, &entry);

    if (entry)
        destroy_oss_node_entry(entry);

    /* Make sure there are no lingering objects */
    oss_node_clean_objects(node);

    kfree((void*)node->name);

    destroy_mutex(node->lock);
    destroy_hashmap(node->obj_map);

    zfree_fixed(_node_allocator, node);
}

/*!
 * @brief: Find the private entry for a given node
 *
 * When @node does not have it's own private field, we walk the parent list
 * until we find a useable field
 */
void* oss_node_unwrap(oss_node_t* node)
{
    if (node->priv)
        return node->priv;

    if (!node->parent)
        return nullptr;

    do {
        node = node->parent;
    } while (node && !node->priv);

    if (!node)
        return nullptr;

    return node->priv;
}

/*!
 * @brief: Add a directory helper struct to a oss node
 */
kerror_t oss_node_attach_dir(oss_node_t* node, dir_t* dir)
{
    if (!node || !dir)
        return -KERR_NULL;

    if (node->dir)
        return -KERR_INVAL;

    mutex_lock(node->lock);

    node->dir = dir;
    dir->node = node;

    mutex_unlock(node->lock);
    return 0;
}

/*!
 * @brief: Same as oss_node_attach_dir, but ignores the exsisting directory
 */
kerror_t oss_node_replace_dir(oss_node_t* node, struct dir* dir)
{
    if (!node || !dir)
        return -KERR_NULL;

    mutex_lock(node->lock);

    /* Detach */
    if (node->dir)
        node->dir->node = nullptr;

    node->dir = dir;
    dir->node = node;

    mutex_unlock(node->lock);
    return 0;
}

kerror_t oss_node_detach_dir(oss_node_t* node)
{
    if (!node || !node->dir)
        return -KERR_NULL;

    mutex_lock(node->lock);

    node->dir->node = nullptr;
    node->dir = nullptr;

    mutex_unlock(node->lock);
    return 0;
}

/*!
 * @brief: Checks if the node has attached objects
 *
 * Caller should take node->lock when calling this function
 */
bool oss_node_is_empty(oss_node_t* node)
{
    return node->obj_map->m_size == 0;
}

/*!
 * @brief: Check if there are any invalid chars in the name
 */
static bool _valid_entry_name(const char* name)
{
    while (*name) {

        /* OSS node entry may not have a path seperator in it's name */
        if (*name == '/')
            return false;

        name++;
    }

    return true;
}

/*!
 * @brief: Check if a certain oss entry already is already added in @node
 *
 * The nodes lock should be taken by the caller. Is also assumed that the
 * caller has verified the integrity of @entry
 */
static inline bool _node_contains_entry(oss_node_t* node, oss_node_entry_t* entry)
{
    const char* entry_name;

    entry_name = oss_node_entry_getname(entry);

    /* How the fuck does this entry not have a name? */
    if (!entry_name)
        return false;

    return (hashmap_get(node->obj_map, (hashmap_key_t)entry_name) != nullptr);
}

/*!
 * @brief: Add a oss_obj to this node
 *
 * This function does not add entries recursively
 */
int oss_node_add_obj(oss_node_t* node, struct oss_obj* obj)
{
    int error;
    oss_node_entry_t* entry;

    if (!node || !obj)
        return -1;

    if (!_valid_entry_name(obj->name))
        return -2;

    entry = create_oss_node_entry(OSS_ENTRY_OBJECT, obj);

    if (!entry)
        return -3;

    mutex_lock(node->lock);

    error = -1;

    if (_node_contains_entry(node, entry))
        goto unlock_and_exit;

    error = (hashmap_put(node->obj_map, (hashmap_key_t)obj->name, entry));

    if (!error)
        obj->parent = node;

unlock_and_exit:
    if (error)
        destroy_oss_node_entry(entry);

    mutex_unlock(node->lock);
    return error;
}

/*!
 * @brief: Add a nested oss node to @node
 *
 * This function does not add entries recursively
 */
int oss_node_add_node(oss_node_t* target, struct oss_node* node)
{
    int error;
    oss_node_entry_t* entry;

    if (!target || !node)
        return -1;

    if (!_valid_entry_name(node->name))
        return -2;

    entry = create_oss_node_entry(OSS_ENTRY_NESTED_NODE, node);

    if (!entry)
        return -3;

    mutex_lock(target->lock);

    error = -1;

    if (_node_contains_entry(target, entry))
        goto unlock_and_exit;

    error = (hashmap_put(target->obj_map, (hashmap_key_t)node->name, entry));

    if (!error)
        node->parent = target;

unlock_and_exit:
    mutex_unlock(target->lock);
    return error;
}

static inline void _entry_on_detach(oss_node_entry_t* entry)
{
    switch (entry->type) {
    case OSS_ENTRY_OBJECT:
        entry->obj->parent = nullptr;
        break;
    case OSS_ENTRY_NESTED_NODE:
        entry->node->parent = nullptr;
        break;
    }
}

/*!
 * @brief: Remove an entry from @node
 *
 * NOTE: when the entry is a nested node, that means that all entries nested in this removed
 * node are also removed from @node
 */
int oss_node_remove_entry(oss_node_t* node, const char* name, struct oss_node_entry** entry_out)
{
    int error;
    oss_node_entry_t* entry;

    if (!node || !name)
        return -1;

    /* If the name is invalid we don't even have to go through all this fuckery */
    if (!_valid_entry_name(name))
        return -2;

    /* Lock because we don't want do die */
    mutex_lock(node->lock);

    /* Remove and get the thing */
    error = -3;
    entry = hashmap_remove(node->obj_map, (hashmap_key_t)name);

    if (!entry)
        goto unlock_and_exit;

    /* Only return if the caller means to */
    if (entry_out)
        *entry_out = entry;

    /* Make sure we tell the entry it's detached */
    _entry_on_detach(entry);
    error = 0;
unlock_and_exit:
    mutex_unlock(node->lock);
    return error;
}

/*!
 * @brief: Find a singular entry inside a node
 *
 * This only searches the nodes object map, regardless of this node being an
 * object generator or not
 */
int oss_node_find(oss_node_t* node, const char* name, struct oss_node_entry** entry_out)
{
    oss_node_entry_t* entry;

    if (!entry_out)
        return -1;

    /* Set to NULL to please fuckers that only check this variable -_- */
    *entry_out = NULL;

    /* If the name is invalid we don't even have to go through all this fuckery */
    if (!_valid_entry_name(name))
        return -2;

    /* Lock because we don't want do die */
    mutex_lock(node->lock);

    /* Get the thing */
    entry = hashmap_get(node->obj_map, (hashmap_key_t)name);

    if (!entry)
        goto unlock_and_exit;

    *entry_out = entry;

    mutex_unlock(node->lock);
    return 0;

unlock_and_exit:
    mutex_unlock(node->lock);
    return -3;
}

/*!
 * @brief: Find the node entry at a specific index
 *
 * This is a heavy opperation. We need to convert the entire hashmap into an array and get the
 * index into that. Use this only when you really need to!
 */
int oss_node_find_at(oss_node_t* node, uint64_t idx, struct oss_node_entry** entry_out)
{
    int error;
    size_t size;
    oss_node_entry_t** array;

    size = NULL;
    array = NULL;

    if (!node || !node->obj_map || idx >= node->obj_map->m_size)
        return -KERR_INVAL;

    error = hashmap_to_array(node->obj_map, (void***)&array, &size);

    if (!KERR_OK(error))
        return error;

    if (!array || !size)
        return -KERR_NULL;

    *entry_out = array[idx];

    kfree(array);
    return 0;
}

int oss_node_query(oss_node_t* node, const char* path, struct oss_obj** obj_out)
{
    if (!obj_out || !node || !node->ops || !node->ops->f_open)
        return -1;

    /* Can't query on a non-gen node =/ */
    if (node->type != OSS_OBJ_GEN_NODE)
        return -2;

    mutex_lock(node->lock);

    *obj_out = node->ops->f_open(node, path);

    mutex_unlock(node->lock);
    return (*obj_out ? 0 : -1);
}

int oss_node_query_node(oss_node_t* node, const char* path, struct oss_node** node_out)
{
    if (!node_out || !node || !node->ops || !node->ops->f_open_node)
        return -1;

    /* Can't query on a non-gen node =/ */
    if (node->type != OSS_OBJ_GEN_NODE)
        return -2;

    mutex_lock(node->lock);

    *node_out = node->ops->f_open_node(node, path);

    mutex_unlock(node->lock);
    return (*node_out ? 0 : -1);
}

/*!
 * @brief: The intermediate callback for oss_node_itterate
 *
 * @arg0: The itteration callback for _node_itter
 * @arg1: The suplementary argument for the itteration
 */
static kerror_t _node_itter(void* v, uint64_t arg0, uint64_t arg1)
{
    int error;
    oss_node_entry_t* entry = v;
    oss_node_t* node;
    oss_obj_t* obj;
    bool (*f_itter)(oss_node_t* node, struct oss_obj* obj, void* arg0) = (void*)arg0;

    if (!entry)
        return (0);

    error = 0;

    switch (entry->type) {
    /* In case of an object, simply call it in the itteration */
    case OSS_ENTRY_OBJECT:
        obj = entry->obj;

        if (!f_itter(nullptr, obj, (void*)arg1))
            return -1;
        break;
    /* Itterate over nodes recursively */
    case OSS_ENTRY_NESTED_NODE:
        node = entry->node;

        if (!f_itter(node, nullptr, (void*)arg1))
            return -1;
        break;
    default:
        break;
    }

    return error ? -1 : (0);
}

/*!
 * @brief: Walk all the child objects under @node
 */
int oss_node_itterate(oss_node_t* node, bool (*f_itter)(oss_node_t* node, struct oss_obj* obj, void* param), void* param)
{
    return (hashmap_itterate(node->obj_map, NULL, _node_itter, (uint64_t)f_itter, (uint64_t)param));
}

/*!
 * @brief: Walks the object map of @node recursively and destroys every object if finds
 */
int oss_node_clean_objects(oss_node_t* node)
{
    int error;
    oss_node_entry_t** array;
    size_t size;
    size_t entry_count;

    mutex_lock(node->lock);

    error = hashmap_to_array(node->obj_map, (void***)&array, &size);

    if (error)
        goto unlock_and_exit;

    entry_count = size / (sizeof(oss_node_entry_t*));

    for (uintptr_t i = 0; i < entry_count; i++) {
        oss_node_entry_t* entry = array[i];

        switch (entry->type) {
        case OSS_ENTRY_NESTED_NODE:
            destroy_oss_node(entry->node);
            break;
        case OSS_ENTRY_OBJECT:
            destroy_oss_obj(entry->obj);
            break;
        }

        destroy_oss_node_entry(entry);
    }

    kfree(array);

unlock_and_exit:
    mutex_unlock(node->lock);
    return error;
}

oss_node_entry_t* create_oss_node_entry(enum OSS_ENTRY_TYPE type, void* obj)
{
    oss_node_entry_t* ret;

    ret = zalloc_fixed(_entry_allocator);

    if (!ret)
        return nullptr;

    memset(ret, 0, sizeof(*ret));

    ret->type = type;
    ret->ptr = obj;

    return ret;
}

void destroy_oss_node_entry(oss_node_entry_t* entry)
{
    zfree_fixed(_entry_allocator, entry);
}
