#include "map.h"
#include "libk/flow/error.h"
#include "var.h"
#include "mem/heap.h"
#include "sync/mutex.h"
#include <libk/data/hashmap.h>
#include <libk/string.h>

sysvar_map_t* create_sysvar_map(uint32_t capacity, uint16_t* p_priv_lvl)
{
  sysvar_map_t* map;

  if (!capacity)
    return nullptr;

  map = kmalloc(sizeof(*map));

  memset(map, 0, sizeof(*map));

  map->p_priv_lvl = p_priv_lvl;
  map->map = create_hashmap(capacity, NULL);
  map->lock = create_mutex(NULL);

  return map;
}

void destroy_sysvar_map(sysvar_map_t* map)
{
  destroy_mutex(map->lock);
  destroy_hashmap(map->map);

  kfree(map);
}

/*!
 * @brief: Get a sysvar from @map
 *
 * Increase the variables refcount
 */
sysvar_t* sysvar_map_get(sysvar_map_t* map, const char* key)
{
  sysvar_t* ret;

  if (!map || !key)
    return nullptr;

  mutex_lock(map->lock);

  ret = hashmap_get(map->map, (hashmap_key_t)key);

  mutex_unlock(map->lock);

  if (!ret)
    return nullptr;

  return get_sysvar(ret);
}

int sysvar_map_put(sysvar_map_t* map, const char* key, enum SYSVAR_TYPE type, uint8_t flags, uintptr_t value)
{
  ErrorOrPtr result;
  sysvar_t* var;

  if (!map || !key)
    return -KERR_INVAL;

  var = create_sysvar(key, type, flags, value);

  if (!var)
    return -KERR_NULL;

  mutex_lock(map->lock);

  result = hashmap_put(map->map, (hashmap_key_t)key, var);

  mutex_unlock(map->lock);

  if (IsError(result)) {
    release_sysvar(var);
    return -KERR_DUPLICATE;
  }

  var->map = map;
  return 0;
}

int sysvar_map_remove(sysvar_map_t* map, const char* key, struct sysvar** bout)
{
  sysvar_t* out;

  if (!map || !key)
    return -KERR_INVAL;

  mutex_lock(map->lock);

  out = hashmap_remove(map->map, (hashmap_key_t)key);

  mutex_unlock(map->lock);

  if (bout)
    *bout = out;

  if (!out)
    return -KERR_NOT_FOUND;

  return 0;
}
