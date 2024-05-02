#include "map.h"
#include "oss/node.h"
#include "oss/obj.h"
#include "sync/atomic_ptr.h"
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
 * @brief: Attach a new sysvar into @node
 *
 *
 */
int sysvar_attach(struct oss_node* node, const char* key, uint16_t priv_lvl, enum SYSVAR_TYPE type, uint8_t flags, uintptr_t value)
{
  int error;
  sysvar_t* target;

  target = create_sysvar(key, priv_lvl, type, flags, value);

  if (!target)
    return -KERR_INVAL;

  error = oss_node_add_obj(node, target->obj);

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

  /* Only return the reference if this variable is still referenced */
  if (atomic_ptr_read(var->refc) > 1 && bvar)
    *bvar = var;

  /* Release a reference */
  release_sysvar(var);

  /* Destroy the middleman */
  destroy_oss_node_entry(entry);

  return 0;
}
