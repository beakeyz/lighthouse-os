#ifndef __ANIVA_HANDLE__
#define __ANIVA_HANDLE__

#include "LibSys/handle_def.h"
#include "libk/error.h"
#include "sync/mutex.h"
#include <libk/stddef.h>

struct file;
struct proc;
struct vobj;
struct aniva_driver;
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
  uint8_t flags;
  uint16_t protection_lvl;
  uint32_t index;
  union {
    struct file* file;
    struct aniva_driver* driver;
    struct proc* process;
    struct virtual_namespace* namespace;
    struct vobj* vobj;
    void* kobj;
  } reference;
} khandle_t;

#define KHNDL_PROT_LVL_TOP          (1200)
#define KHNDL_PROT_LVL_HIGH         (800)
#define KHNDL_PROT_LVL_MEDIUM       (500)
#define KHDNL_PROT_LVL_LOW          (250)
#define KHDNL_PROT_LVL_LOWEST       (0)

#define KHNDL_FLAG_BUSY             (0x01) /* khandle is busy and we have given up waiting for it */
#define KHNDL_FLAG_WAITING          (0x02) /* khandle is busy but we are waiting for it to be free */
#define KHNDL_FLAG_INVALID          (0x80) /* khandle is not pointing to anything and any accesses to it should be regarded as disbehaviour */

#define KHNDL_TYPE_NONE             (0)
#define KHNDL_TYPE_FILE             (1)
#define KHNDL_TYPE_DRIVER           (2)
#define KHNDL_TYPE_PROC             (3)
#define KHNDL_TYPE_FS_ROOT          (4)
#define KHNDL_TYPE_VOBJ             (5)
#define KHNDL_TYPE_KOBJ             (6)

void create_khandle(khandle_t* out_handle, khandle_type_t* type, void* ref);
void destroy_khandle(khandle_t* handle);

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

/* Cleans up the maps handles */
void destroy_khandle_map(khandle_map_t* map);

/* NOTE: mutates the handle to fill in the index they are put at */
ErrorOrPtr bind_khandle(khandle_map_t* map, khandle_t* handle);

/* NOTE: mutates the handle to clear the index */
ErrorOrPtr unbind_khandle(khandle_map_t* map, khandle_t* handle);

khandle_t* find_khandle(khandle_map_t* map, uint32_t index);

/*
 * Mutate the handle that lives in the index specified
 */
ErrorOrPtr mutate_khandle(khandle_map_t* map, khandle_t handle, uint32_t index);

#endif // !__ANIVA_HANDLE__
