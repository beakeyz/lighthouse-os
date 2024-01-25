#ifndef __ANIVA_OSS_NODE__
#define __ANIVA_OSS_NODE__

#include "sync/mutex.h"
#include <libk/stddef.h>
#include <libk/data/hashmap.h>

struct vobj;
struct oss_node_entry;
struct oss_node_ops;

enum OSS_NODE_TYPE {
  /* This node generates vobjects through a filesystem */
  OSS_OBJ_GEN_NODE,
  /* This node holds objects */
  OSS_OBJ_STORE_NODE,
};

void init_oss_nodes();

/*
 * Base struct for oss nodes
 *
 * oss nodes are either vobj storage containers or vobj generators
 */
typedef struct oss_node {
  const char* name;
  enum OSS_NODE_TYPE type;

  struct oss_node_ops* ops;
  struct oss_node* parent;

  mutex_t* lock;
  hashmap_t* obj_map;
} oss_node_t;

oss_node_t* create_oss_node(const char* name, enum OSS_NODE_TYPE type, struct oss_node_ops* ops, struct oss_node* parent);
void destroy_oss_node(oss_node_t* node);

enum OSS_ENTRY_TYPE {
  OSS_ENTRY_NESTED_NODE,
  OSS_ENTRY_OBJECT,
};

int oss_node_add(oss_node_t* node, enum OSS_ENTRY_TYPE type, void* obj, const char* path);
int oss_node_remove(oss_node_t* node, const char* path, enum OSS_ENTRY_TYPE* type_out, void** obj_out);

/*
 * Entries inside the object map
 */
typedef struct oss_node_entry {
  enum OSS_ENTRY_TYPE type;

  union {
    struct oss_node* node;
    struct vobj* obj;
  };
} oss_node_entry_t;

oss_node_entry_t* create_oss_node_entry(enum OSS_ENTRY_TYPE type, void* obj);
void destroy_oss_node_entry(oss_node_entry_t* entry);

/*
 * Ops for nodes that generate their own vobjs (Like filesystems, or driver endpoints)
 * 
 * TODO: Make complete
 */
typedef struct oss_node_ops {
  /* Send some data to this node and have whatever is connected do something for you */
  int (*f_msg) (struct oss_node*, driver_control_code_t code, void* buffer, size_t size);
  /* Grab named data associated with this node */
  struct vobj* (*f_open) (struct oss_node*, char*);
  /* Close a vobject that has been opened by this vnode */
  int (*f_close)(struct oss_node*, struct vobj*); 
} oss_node_ops_t;

#endif // !__ANIVA_OSS_NODE__
