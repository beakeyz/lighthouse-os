#include "map.h"
#include "mem/heap.h"
#include "oss/node.h"
#include "oss/obj.h"
#include "system/profile/attr.h"
#include "system/sysvar/var.h"

bool oss_node_can_contain_sysvar(oss_node_t* node)
{
    return (node->type == OSS_PROFILE_NODE || node->type == OSS_PROC_ENV_NODE || node->type == OSS_VMEM_NODE);
}

/*!
 * @brief: Grab a sysvar from a node
 *
 * Try to find a sysvar at @key inside @node
 */
sysvar_t* sysvar_get(oss_node_t* node, const char* key)
{
    sysvar_t* ret;
    oss_obj_t* obj;
    oss_node_entry_t* entry;

    if (!node || !key)
        return nullptr;

    if (KERR_ERR(oss_node_find(node, key, &entry)))
        return nullptr;

    if (entry->type != OSS_ENTRY_OBJECT)
        return nullptr;

    obj = entry->obj;

    if (obj->type != OSS_OBJ_TYPE_VAR)
        return nullptr;

    ret = oss_obj_unwrap(obj, sysvar_t);

    /* Return with a taken reference */
    return get_sysvar(ret);
}

/*!
 * @brief: Itterator function for counting the amount of variables for sysvar_dump
 *
 * @node: Null if the current item is not a node, otherwise current item
 * @obj: Null if the current item is not an obj, otherwise current item
 * @param: pointer to the size variable
 */
static bool sysvar_node__count_itter(oss_node_t* node, oss_obj_t* obj, void* param)
{
    if (!obj)
        return true;

    if (obj->type != OSS_OBJ_TYPE_VAR)
        return true;

    /* Increase the node count */
    (*(size_t*)param)++;
    return true;
}

/*!
 * @brief: Itterator function for adding variables to a list
 *
 * @node: Null if the current item is not a node, otherwise current item
 * @obj: Null if the current item is not an obj, otherwise current item
 * @param: pointer to the variable list
 */
static bool sysvar_node__add_itter(oss_node_t* node, oss_obj_t* obj, void* param)
{
    sysvar_t*** p_arr;

    if (!obj)
        return true;

    if (obj->type != OSS_OBJ_TYPE_VAR)
        return true;

    p_arr = param;

    /* Set this entry */
    **p_arr = obj->priv;

    /* Move the pointer 'head' */
    (*p_arr)++;

    /* Go next */
    return true;
}

/*!
 * @brief: Dump all the variables on @node
 *
 * Leaks weak references to the variables. This function expects @node to be locked
 */
int sysvar_dump(struct oss_node* node, struct sysvar*** barr, size_t* bsize)
{
    int error;
    sysvar_t** var_arr_start;
    sysvar_t** var_arr;

    if (!node || !barr || !bsize)
        return -1;

    /* Quick reset */
    *bsize = NULL;

    /* First, count the variables on this node */
    error = oss_node_itterate(node, sysvar_node__count_itter, bsize);

    if (error)
        return error;

    if (!(*bsize))
        return -KERR_NOT_FOUND;

    /* Allocate the list */
    var_arr = kmalloc(sizeof(sysvar_t*) * (*bsize));

    if (!(*barr))
        return -KERR_NOMEM;

    /* Keep the beginning of the array here, since var_arr will get mutated */
    var_arr_start = var_arr;

    /* Try to itterate */
    error = oss_node_itterate(node, sysvar_node__add_itter, var_arr);

    /* Yikes */
    if (error) {
        kfree(var_arr_start);
        return error;
    }

    /* Set the param */
    *barr = var_arr_start;
    return 0;
}

int sysvar_attach(struct oss_node* node, struct sysvar* var)
{
    if (!var)
        return -EINVAL;

    /* Only these types may have sysvars */
    if (node->type != OSS_OBJ_STORE_NODE && node->type != OSS_VMEM_NODE && node->type != OSS_PROFILE_NODE && node->type != OSS_PROC_ENV_NODE)
        return -EINVAL;

    return oss_node_add_obj(node, var->obj);
}

/*!
 * @brief: Attach a new sysvar into @node
 *
 *
 */
int sysvar_attach_ex(struct oss_node* node, const char* key, enum PROFILE_TYPE ptype, enum SYSVAR_TYPE type, uint8_t flags, void* buffer, size_t bsize)
{
    error_t error;
    sysvar_t* target;

    /*
     * Initialize a permission attributes struct for a generic sysvar. By default
     * sysvars can be seen and read from by everyone.
     */
    pattr_t pattr = {
        .ptype = ptype,
        .pflags = PATTR_UNIFORM_FLAGS(PATTR_SEE | PATTR_READ),
    };

    /* Make sure anyone from the same level can also mutate this fucker */
    pattr.pflags[ptype] |= (PATTR_WRITE | PATTR_DELETE);

    /* Try to create this fucker */
    target = create_sysvar(key, &pattr, type, flags, buffer, bsize);

    if (!target)
        return -ENOMEM;

    error = sysvar_attach(node, target);

    /* This is kinda yikes lol */
    if (error)
        release_sysvar(target);

    return error;
}

/*!
 * @brief: Remove a sysvar through its oss obj
 */
int sysvar_detach(struct oss_node* node, const char* key, struct sysvar** bvar)
{
    int error;
    sysvar_t* var;
    oss_node_entry_t* entry = NULL;

    error = oss_node_remove_entry(node, key, &entry);

    if (error)
        return error;

    if (entry->type != OSS_ENTRY_OBJECT)
        return -KERR_NOT_FOUND;

    var = oss_obj_unwrap(entry->obj, sysvar_t);

    sysvar_lock(var);

    /* Only return the reference if this variable is still referenced */
    if (var->refc > 1 && bvar)
        *bvar = var;

    sysvar_unlock(var);

    /* Release a reference */
    release_sysvar(var);

    /* Destroy the middleman */
    destroy_oss_node_entry(entry);

    return 0;
}
