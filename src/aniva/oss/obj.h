#ifndef __ANIVA_OSS_OBJ__
#define __ANIVA_OSS_OBJ__

#include "sync/mutex.h"
#include <libk/stddef.h>

struct oss_obj;
struct oss_node;

typedef struct oss_obj_ops {
  void (*f_destory_priv)(void* obj);
} oss_obj_ops_t;


enum OSS_OBJ_TYPE {
  OSS_OBJ_TYPE_EMPTY = 0,
  /* Generic files in the filesystem get this */
  OSS_OBJ_TYPE_FILE,
  /* A directory can be extracted from a filesystem, in which case it gets a oss_obj */
  OSS_OBJ_TYPE_DIR,
  /* When we obtain a device */
  OSS_OBJ_TYPE_DEVICE,
  /* When we obtain a driver */
  OSS_OBJ_TYPE_DRIVER,
  /* This boi links to another oss_obj */
  OSS_OBJ_TYPE_LINK,
};

#define OSS_OBJ_IMMUTABLE  0x00000001 /* Can we change the data that this object holds? */
#define OSS_OBJ_CONFIG     0x00000002 /* Does this object point to configuration? */
#define OSS_OBJ_SYS        0x00000004 /* Is this object owned by the system? */
#define OSS_OBJ_ETERNAL    0x00000008 /* Does this oss_obj ever get cleaned? */
#define OSS_OBJ_MOVABLE    0x00000010 /* Can this object be moved to different vnodes? */
#define OSS_OBJ_REF        0x00000020 /* Does this object reference another object? */

typedef struct oss_obj {
  const char* name;
  uint32_t flags;
  enum OSS_OBJ_TYPE type;

  mutex_t* lock;
  struct oss_node* parent;
  struct oss_obj_ops ops;
  
  void* priv;
} oss_obj_t;

#define oss_obj_unwrap(obj, type) (type*)(obj->priv)

oss_obj_t* create_oss_obj(const char* name);
void destroy_oss_obj(oss_obj_t* obj);

void oss_obj_ref(oss_obj_t* obj);
int oss_obj_unref(oss_obj_t* obj);

const char* oss_obj_get_fullpath(oss_obj_t* obj);

void oss_obj_register_child(oss_obj_t* obj, void* child, enum OSS_OBJ_TYPE type, FuncPtr destroy_fn);

#endif // !__ANIVA_OSS_OBJ__
