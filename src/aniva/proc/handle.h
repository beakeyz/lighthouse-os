#ifndef __ANIVA_HANDLE__
#define __ANIVA_HANDLE__

#include "LibSys/handle_def.h"
#include "libk/flow/error.h"
#include "sync/mutex.h"
#include <libk/stddef.h>

struct file;
struct proc;
struct thread;
struct vobj;
struct profile_var;
struct proc_profile;
struct dev_manifest;
struct virtual_namespace;

typedef uint8_t khandle_type_t;

/*
 * A kernel-side handle.
 *
 * This object the reflection of the userspace handles that are handed out, which 
 * simply act as indices to these kind of handles.
 */
typedef struct kernel_handle {
  khandle_type_t type;
  uint8_t protection_lvl;
  uint16_t flags;
  uint32_t index;
  uintptr_t offset;
  union {
    struct file* file;
    struct dev_manifest* driver;
    struct proc* process;
    struct thread* thread;
    struct virtual_namespace* namespace;
    struct vobj* vobj;
    struct proc_profile* profile;
    struct profile_var* pvar;
    void* kobj;
  } reference;
} khandle_t;

#define KHNDL_PROT_LVL_TOP          (0xFF)
#define KHNDL_PROT_LVL_HIGH         (150)
#define KHNDL_PROT_LVL_MEDIUM       (100)
#define KHDNL_PROT_LVL_LOW          (50)
#define KHDNL_PROT_LVL_LOWEST       (0)

void create_khandle(khandle_t* out_handle, khandle_type_t* type, void* ref);
void destroy_khandle(khandle_t* handle);

void khandle_set_flags(khandle_t* handle, uint16_t flags);

#define KHNDL_MAX_ENTRIES          (1024) /* The current maximum amount of handles (same as linux) */
#define KHNDL_DEFAULT_ENTRIES      (512)

#define KHNDL_INVALID_INDEX        (uint32_t)-1

/*
 * Map that stores the handles for a specific entity
 * NOTE: should never be heap-allocated on its own
 */
typedef struct khandle_map {
  khandle_t* handles;
  uint32_t next_free_index;
  uint32_t count;
  uint32_t max_count; /* Hard limit for this value is KHANDL_MAX_ENTRIES, but we can also limit a process even further if that is needed */
  mutex_t* lock;
} khandle_map_t;

/* Unconventional create, since this does not use the heap */
khandle_map_t create_khandle_map_ex(uint32_t max_count);
void init_khandle_map(khandle_map_t* map, uint32_t max_count);

/* Cleans up the maps handles */
void destroy_khandle_map(khandle_map_t* map);

/* NOTE: mutates the handle to fill in the index they are put at */
ErrorOrPtr bind_khandle(khandle_map_t* map, khandle_t* handle);

ErrorOrPtr bind_khandle_at(khandle_map_t* map, khandle_t* handle, uint32_t index);
ErrorOrPtr try_bind_khandle_at(khandle_map_t* map, khandle_t* handle, uint32_t index);


/* NOTE: mutates the handle to clear the index */
ErrorOrPtr unbind_khandle(khandle_map_t* map, khandle_t* handle);

khandle_t* find_khandle(khandle_map_t* map, uint32_t index);

/*
 * Mutate the handle that lives in the index specified
 */
ErrorOrPtr mutate_khandle(khandle_map_t* map, khandle_t handle, uint32_t index);

#endif // !__ANIVA_HANDLE__
