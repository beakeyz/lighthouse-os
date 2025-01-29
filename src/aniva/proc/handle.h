#ifndef __ANIVA_HANDLE__
#define __ANIVA_HANDLE__

#include "libk/flow/error.h"
#include "sync/mutex.h"
#include <libk/stddef.h>

struct thread;
struct oss_object;

/*
 * A kernel-side handle.
 *
 * This object the reflection of the userspace handles that are handed out, which
 * simply act as indices to these kind of handles.
 */
typedef struct kernel_handle {
    uint32_t index;
    uint8_t protection_lvl;
    uint8_t res;
    uint32_t flags;
    struct oss_object* object;
} khandle_t;

#define KHNDL_PROT_LVL_TOP (0xFF)
#define KHNDL_PROT_LVL_HIGH (150)
#define KHNDL_PROT_LVL_MEDIUM (100)
#define KHDNL_PROT_LVL_LOW (50)
#define KHDNL_PROT_LVL_LOWEST (0)

void init_khandle(khandle_t* out_handle, struct oss_object* ref);
void init_khandle_ex(khandle_t* bHandle, u32 flags, struct oss_object* ref);
void destroy_khandle(khandle_t* handle);
void khandle_set_flags(khandle_t* handle, uint32_t flags);

#define KHNDL_MAX_ENTRIES (1024) /* The current maximum amount of handles (same as linux) */
#define KHNDL_DEFAULT_ENTRIES (512)

#define KHNDL_INVALID_INDEX (uint32_t)-1

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
int init_khandle_map(khandle_map_t* map, uint32_t max_count);
int copy_khandle_map(khandle_map_t* dst_map, khandle_map_t* src_map);

/* Cleans up the maps handles */
void destroy_khandle_map(khandle_map_t* map);

/* NOTE: mutates the handle to fill in the index they are put at */
kerror_t bind_khandle(khandle_map_t* map, khandle_t* handle, uint32_t* bidx);

kerror_t bind_khandle_at(khandle_map_t* map, khandle_t* handle, uint32_t index);
kerror_t try_bind_khandle_at(khandle_map_t* map, khandle_t* handle, uint32_t index);

/* NOTE: mutates the handle to clear the index */
kerror_t unbind_khandle(khandle_map_t* map, u32 handle);

khandle_t* find_khandle(khandle_map_t* map, uint32_t index);

#endif // !__ANIVA_HANDLE__
