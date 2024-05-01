#ifndef __ANIVA_OSS_OBJ__
#define __ANIVA_OSS_OBJ__

#include "sync/atomic_ptr.h"
#include "sync/mutex.h"
#include <libk/stddef.h>
#include <system/profile/profile.h>

struct oss_obj;
struct oss_node;
struct file;
struct device;

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
#define OSS_OBJ_MOVABLE    0x00000010 /* Can this object be moved to different nodes? */
#define OSS_OBJ_REF        0x00000020 /* Does this object reference another object? */

typedef struct oss_obj {
  const char* name;

  atomic_ptr_t refc;

  /* Required privilge level to access this object */
  uint8_t access_priv_lvl;
  /* Required privilge level to read from this object */
  uint8_t read_priv_lvl;
  /* Required privilge level to write to this object */
  uint8_t write_priv_lvl;

  uint32_t flags;
  enum OSS_OBJ_TYPE type;

  mutex_t* lock;
  struct oss_node* parent;
  struct oss_obj_ops ops;
  
  void* priv;
} oss_obj_t;

#define oss_obj_unwrap(obj, type) (type*)(obj->priv)

#define __oss_obj_can_proc(obj, p, thing) ((p)->m_env->priv_level >= (obj)->thing##_priv_lvl)
#define oss_obj_can_proc_access(obj, p) __oss_obj_can_proc(obj, p, access)
#define oss_obj_can_proc_read(obj, p) __oss_obj_can_proc(obj, p, read)
#define oss_obj_can_proc_write(obj, p) __oss_obj_can_proc(obj, p, write)

oss_obj_t* create_oss_obj(const char* name);
oss_obj_t* create_oss_obj_ex(const char* name, uint32_t priv_lvl);
void destroy_oss_obj(oss_obj_t* obj);

void oss_obj_ref(oss_obj_t* obj);
void oss_obj_unref(oss_obj_t* obj);

int oss_obj_close(oss_obj_t* obj);

int oss_obj_rename(oss_obj_t* obj, const char* newname);

const char* oss_obj_get_fullpath(oss_obj_t* obj);
void oss_obj_register_child(oss_obj_t* obj, void* child, enum OSS_OBJ_TYPE type, FuncPtr destroy_fn);

struct oss_node* oss_obj_get_root_parent(oss_obj_t* obj);

static inline struct file* oss_obj_get_file(oss_obj_t* obj)
{
  if (obj->type != OSS_OBJ_TYPE_FILE)
    return nullptr;

  return oss_obj_unwrap(obj, struct file);
}

static inline struct device* oss_obj_get_device(oss_obj_t* obj)
{
  if (obj->type != OSS_OBJ_TYPE_DEVICE)
    return nullptr;

  return oss_obj_unwrap(obj, struct device);
}

#endif // !__ANIVA_OSS_OBJ__
