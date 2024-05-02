#ifndef __ANIVA_OSS_NODE__
#define __ANIVA_OSS_NODE__

#include "dev/core.h"
#include "libk/flow/error.h"
#include "oss/obj.h"
#include "sync/mutex.h"
#include <libk/stddef.h>
#include <libk/data/hashmap.h>

struct dir;
struct oss_obj;
struct oss_node_entry;
struct oss_node_ops;

enum OSS_NODE_TYPE {
  /* This node generates oss_objects through a filesystem (TODO: rename to OSS_FS_NODE ?) */
  OSS_OBJ_GEN_NODE,
  /* This node holds objects */
  OSS_OBJ_STORE_NODE,
  /* (TODO) This node holds a link to another node */
  OSS_LINK_NODE,
  /* This node holds a groep for devices */
  OSS_GROUP_NODE,
  /* User Profiles stored in a node */
  OSS_PROFILE_NODE,
  /* Node that holds an environment */
  OSS_PROC_ENV_NODE,
};

void init_oss_nodes();

/*
 * Base struct for oss nodes
 *
 * oss nodes are either oss_obj storage containers or vobj generators
 *
 * Every node can have a directory attached
 */
typedef struct oss_node {
  const char* name;
  enum OSS_NODE_TYPE type;

  struct oss_node_ops* ops;
  struct oss_node* parent;

  mutex_t* lock;
  hashmap_t* obj_map;

  struct dir* dir;
  void* priv;
} oss_node_t;

oss_node_t* create_oss_node(const char* name, enum OSS_NODE_TYPE type, struct oss_node_ops* ops, struct oss_node* parent);
void destroy_oss_node(oss_node_t* node);

void* oss_node_unwrap(oss_node_t* node);

kerror_t oss_node_attach_dir(oss_node_t* node, struct dir* dir);
kerror_t oss_node_replace_dir(oss_node_t* node, struct dir* dir);
kerror_t oss_node_detach_dir(oss_node_t* node);

enum OSS_ENTRY_TYPE {
  OSS_ENTRY_NESTED_NODE,
  OSS_ENTRY_OBJECT,
};

int oss_node_add_obj(oss_node_t* node, struct oss_obj* obj);
int oss_node_add_node(oss_node_t* node, struct oss_node* obj);
int oss_node_remove_entry(oss_node_t* node, const char* name, struct oss_node_entry** entry_out);

int oss_node_find(oss_node_t* node, const char* name, struct oss_node_entry** entry_out);
int oss_node_find_at(oss_node_t* node, uint64_t idx, struct oss_node_entry** entry_out);
int oss_node_query(oss_node_t* node, const char* path, struct oss_obj** obj_out);
int oss_node_query_node(oss_node_t* node, const char* path, struct oss_node** node_out);

int oss_node_itterate(oss_node_t* node, bool(*f_itter)(oss_node_t* node, struct oss_obj* obj, void* param), void* param);

int oss_node_clean_objects(oss_node_t* node);

/*
 * Entries inside the object map
 */
typedef struct oss_node_entry {
  enum OSS_ENTRY_TYPE type;

  union {
    struct oss_node* node;
    struct oss_obj* obj;

    void* ptr;
  };
} oss_node_entry_t;

oss_node_entry_t* create_oss_node_entry(enum OSS_ENTRY_TYPE type, void* obj);
void destroy_oss_node_entry(oss_node_entry_t* entry);

static inline const char* oss_node_entry_getname(oss_node_entry_t* entry)
{
  switch (entry->type) {
    case OSS_ENTRY_NESTED_NODE:
      return entry->node->name;
    case OSS_ENTRY_OBJECT:
      return entry->obj->name;
    default:
      break;
  }

  return nullptr;
}

/*
 * Ops for nodes that generate their own oss_objs (Like filesystems, or driver endpoints)
 * 
 * TODO: Make complete
 */
typedef struct oss_node_ops {
  /*
   * Send some data to this node and have whatever is connected do something for you 
   */
  int (*f_msg) (struct oss_node*, driver_control_code_t code, void* buffer, size_t size);
  /*
   * Force an object to be synced 
   */
  int (*f_force_obj_sync)(struct oss_obj*);
  /*
   * Grab named data associated with this node 
   */
  struct oss_obj* (*f_open) (struct oss_node*, const char*);
  /*
   * Grab a nested node from a parent node 
   */
  struct oss_node* (*f_open_node) (struct oss_node*, const char*);
  /*
   * Close a oss_object that has been opened by this node 
   */
  int (*f_close)(struct oss_node*, struct oss_obj*); 
  /*
   * Destroy an oss node 
   */
  int (*f_destroy)(struct oss_node*);
} oss_node_ops_t;

#endif // !__ANIVA_OSS_NODE__
