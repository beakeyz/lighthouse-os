#ifndef __ANIVA_SYSVAR_MAP__
#define __ANIVA_SYSVAR_MAP__

#include "libk/data/hashmap.h"
#include "lightos/proc/var_types.h"
#include "sync/mutex.h"

struct sysvar;

/*
 * Wrapper around a hashmap that stores sysvars
 *
 * Meant to be load- and saveable
 */
typedef struct sysvar_map {
  hashmap_t* map;
  mutex_t* lock;
  uint16_t* p_priv_lvl;
} sysvar_map_t;

sysvar_map_t* create_sysvar_map(uint32_t capacity, uint16_t* p_priv_lvl);
void destroy_sysvar_map(sysvar_map_t* map);

struct sysvar* sysvar_map_get(sysvar_map_t* map, const char* key);
int sysvar_map_put(sysvar_map_t* map, const char* key, enum SYSVAR_TYPE type, uint8_t flags, uintptr_t value);
int sysvar_map_remove(sysvar_map_t* map, const char* key, struct sysvar** bout);

static inline bool sysvar_map_can_access(sysvar_map_t* map, uint16_t prv_lvl)
{
  /* lol */
  if (!map->p_priv_lvl)
    return true;

  /* Can access if the map has a lower privilege level */
  return (prv_lvl >= *map->p_priv_lvl);
}

#endif // !__ANIVA_SYSVAR_MAP__
