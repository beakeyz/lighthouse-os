#include "map.h"
#include "mem/heap.h"
#include "oss/node.h"
#include "oss/obj.h"
#include "system/sysvar/var.h"

bool oss_node_can_contain_sysvar(oss_node_t* node)
{
    return (node->type == OSS_PROFILE_NODE || node->type == OSS_PROC_ENV_NODE);
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

    if (!oss_node_can_contain_sysvar(node))
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
    error_t error;

    if (!var)
        return -KERR_INVAL;

    error = oss_node_add_obj(node, var->obj);

    /* This is kinda yikes lol */
    if (error)
        release_sysvar(var);

    return error;
}

/*!
 * @brief: Attach a new sysvar into @node
 *
 *
 */
int sysvar_attach_ex(struct oss_node* node, const char* key, enum PROFILE_TYPE ptype, enum SYSVAR_TYPE type, uint8_t flags, void* buffer, size_t bsize)
{
    sysvar_t* target;

    /* Try to create this fucker */
    target = create_sysvar(key, ptype, type, flags, buffer, bsize);

    return sysvar_attach(node, target);
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
