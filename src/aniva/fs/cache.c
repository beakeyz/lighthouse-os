#include "cache.h"
#include "libk/data/hive.h"
#include "libk/data/linkedlist.h"

hive_t* s_node_base_cache;
hive_t* s_read_cache;

void init_vcache() {

  s_node_base_cache = create_hive("base_cache");
  // TODO:
}

void cache_vnode(vnode_t node) {

}

bool is_vnode_cached(vnode_t* node) {
  return false;
}
