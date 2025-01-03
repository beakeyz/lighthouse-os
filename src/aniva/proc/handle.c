#include "handle.h"
#include "fs/dir.h"
#include "fs/file.h"
#include "libk/flow/error.h"
#include "lightos/handle_def.h"
#include "logging/log.h"
#include "oss/obj.h"
#include "proc/hdrv/driver.h"
#include "sync/mutex.h"
#include "system/sysvar/var.h"
#include <dev/driver.h>
#include <mem/kmem.h>
#include <proc/env.h>
#include <proc/proc.h>

void init_khandle(khandle_t* out_handle, HANDLE_TYPE* type, void* ref)
{
    if (!ref)
        return;

    memset(out_handle, 0, sizeof(khandle_t));

    out_handle->index = KHNDL_INVALID_INDEX;
    out_handle->type = *type;
    out_handle->reference.kobj = ref;
}

void init_khandle_ex(khandle_t* bHandle, HANDLE_TYPE type, u32 flags, void* ref)
{
    init_khandle(bHandle, &type, ref);

    khandle_set_flags(bHandle, flags);
}

static inline void __destroy_khandle(khandle_t* handle)
{
    memset(handle, 0, sizeof(khandle_t));
    handle->index = KHNDL_INVALID_INDEX;
}

void destroy_khandle(khandle_t* handle)
{
    khandle_driver_t* hdrv;

    if (!handle)
        return;

    if (khandle_driver_find(handle->type, &hdrv))
        goto destroy_and_exit;

    /* Make sure to handle the state change on this handle inside the object */
    khandle_driver_close(hdrv, handle);

destroy_and_exit:
    /* Actually clear the handle internally */
    __destroy_khandle(handle);
}

/*!
 * @brief Set the flags of a khandle and mask them
 *
 * Nothing to add here...
 */
void khandle_set_flags(khandle_t* handle, uint32_t flags)
{
    if (!handle)
        return;

    handle->flags = flags;
}

struct oss_node* khandle_get_relative_node(khandle_t* handle)
{
    switch (handle->type) {
    case HNDL_TYPE_PROC:
        if (!handle->reference.process->m_env)
            return NULL;

        return handle->reference.process->m_env->node;
    case HNDL_TYPE_PROC_ENV:
        return handle->reference.penv->node;
    case HNDL_TYPE_DIR:
        return handle->reference.dir->node;
    case HNDL_TYPE_PROFILE:
        return handle->reference.profile->node;
        break;
    default:
        break;
    }

    return NULL;
}

/*
 * Check if the handle seems bound by looking for its index and reference
 */
static bool __is_khandle_bound(khandle_t* handle)
{
    return (handle != nullptr && ((handle->index != KHNDL_INVALID_INDEX) && handle->reference.kobj != nullptr));
}

int init_khandle_map(khandle_map_t* ret, uint32_t max_count)
{
    int error;

    if (!ret)
        return -1;

    if (!max_count || max_count > KHNDL_MAX_ENTRIES)
        max_count = KHNDL_DEFAULT_ENTRIES;

    memset(ret, 0, sizeof(khandle_map_t));

    ret->max_count = max_count;
    ret->lock = create_mutex(0);

    /* Ensure we can create this list that provides a maximum of 1024 handles */
    error = kmem_kernel_alloc_range((void**)&ret->handles, max_count * sizeof(khandle_t), NULL, KMEM_FLAG_WRITABLE | KMEM_FLAG_KERNEL);

    if (error) {
        destroy_mutex(ret->lock);
        return error;
    }

    /* Zero */
    memset(ret->handles, 0, max_count * sizeof(khandle_t));

    /* Should we? */
    for (uint32_t i = 0; i < ret->max_count; i++)
        ret->handles[i].index = KHNDL_INVALID_INDEX;

    return 0;
}

/*!
 * @brief: Copy an entire khandle map into a new map
 */
int copy_khandle_map(khandle_map_t* dst_map, khandle_map_t* src_map)
{
    int error;

    if (!dst_map || !src_map)
        return -KERR_INVAL;

    mutex_lock(src_map->lock);

    error = init_khandle_map(dst_map, src_map->max_count);

    if (error)
        goto unlock_and_error;

    dst_map->count = src_map->count;
    dst_map->next_free_index = src_map->next_free_index;

    /* Copy the handles */
    memcpy(dst_map->handles, src_map->handles, sizeof(khandle_t) * dst_map->max_count);

unlock_and_error:
    mutex_unlock(src_map->lock);
    return error;
}

khandle_map_t create_khandle_map_ex(uint32_t max_count)
{
    khandle_map_t ret;

    init_khandle_map(&ret, max_count);

    return ret;
}

/* Cleans up the maps handles */
void destroy_khandle_map(khandle_map_t* map)
{
    if (!map)
        return;

    KLOG_INFO("Destroying khandle map! (0x%p)\n", map->handles);

    destroy_mutex(map->lock);

    ASSERT_MSG(map->max_count, "Tried to destroy a khandle map without a max_count!");

    ASSERT_MSG(kmem_kernel_dealloc((vaddr_t)map->handles, map->max_count * sizeof(khandle_t)) == 0, "Failed to dealloc khandle map");
}

khandle_t* find_khandle(khandle_map_t* map, uint32_t user_handle)
{
    khandle_t* handle = nullptr;

    mutex_lock(map->lock);

    if (!map)
        goto unlock_and_exit;

    if (user_handle >= map->max_count)
        goto unlock_and_exit;

    if (!__is_khandle_bound(&map->handles[user_handle]))
        goto unlock_and_exit;

    handle = &map->handles[user_handle];

unlock_and_exit:
    mutex_unlock(map->lock);
    return handle;
}

static kerror_t __try_reallocate_handles(khandle_map_t* map, size_t new_max_count)
{
    kerror_t error;
    khandle_t* new_list;

    /* Debug */
    kernel_panic("TODO: had to reallocate khandles! verify this works (__try_reallocate_handles)");

    if (!map || !new_max_count || new_max_count > KHNDL_MAX_ENTRIES)
        return -1;

    /* Allocate new space */
    error = kmem_kernel_alloc_range((void**)&new_list, new_max_count * sizeof(khandle_t), NULL, KMEM_FLAG_WRITABLE | KMEM_FLAG_KERNEL);

    if (error)
        return error;

    /* Since they are stored as one massive vector, we can simply copy all of it over */
    memcpy(map->handles, new_list, map->max_count * sizeof(khandle_t));

    /* We can now deallocate the old list */
    error = kmem_kernel_dealloc((uint64_t)map->handles, map->max_count * sizeof(khandle_t));

    /* Update max_count */
    map->max_count = new_max_count;

    return (error);
}

static void __bind_khandle(khandle_map_t* map, khandle_t* handle, uint32_t index)
{
    if (!map || !handle)
        return;

    /* First mutate the index */
    handle->index = index;

    /* Then copy it over */
    // map->handles[index] = *handle;
    memcpy(&map->handles[index], handle, sizeof(khandle_t));
    map->count++;
}

/*!
 * @brief: Copies a handle into the handle map
 *
 * NOTE: mutates the handle to fill in the index they are put at
 */
kerror_t bind_khandle(khandle_map_t* map, khandle_t* handle, uint32_t* bidx)
{
    kerror_t res;
    uint32_t current_index;
    khandle_t* slot;

    if (!map || !handle)
        return -1;

    mutex_lock(map->lock);

    if (map->count >= map->max_count) {

        /* Have we reached the hardlimit? */
        if (map->max_count >= KHNDL_MAX_ENTRIES)
            return -1;

        /* __try_reallocate_handles will mutate the values in map */
        res = __try_reallocate_handles(map, KHNDL_MAX_ENTRIES);

        /* If we fail to reallocate, thats kinda cringe lol */
        if ((res)) {
            mutex_unlock(map->lock);
            return res;
        }
    }

    /* Check if the next free index is valid */
    current_index = map->next_free_index;

    /* Try to get the next free index */
    if (current_index != KHNDL_INVALID_INDEX) {

        slot = &map->handles[current_index];

        if (!__is_khandle_bound(slot)) {

            /* insert into array */
            __bind_khandle(map, handle, current_index);

            map->next_free_index += 1;
            goto unlock_and_success;
        }
    }

    /* Check if we can put it somewhere in a hole */
    current_index = 0;

    /* Loop over the entire array */
    while (current_index < map->max_count) {

        slot = &map->handles[current_index];

        if (!__is_khandle_bound(slot)) {

            __bind_khandle(map, handle, current_index);

            /* Make sure that the next free index is still valid */
            map->next_free_index = current_index + 1;
            goto unlock_and_success;
        }

        current_index++;
    }

    /* Could not find a place to bind this handle, abort */
    mutex_unlock(map->lock);
    return -1;

unlock_and_success:
    mutex_unlock(map->lock);

    if (bidx)
        *bidx = current_index;
    return (0);
}

/* NOTE: mutates the handle to clear the index */
kerror_t unbind_khandle(khandle_map_t* map, khandle_t* handle)
{
    khandle_t* _handle;

    mutex_lock(map->lock);

    if (!map || !handle)
        goto error_and_unlock;

    _handle = &map->handles[handle->index];

    /* WTF */
    if (_handle != handle)
        goto error_and_unlock;

    /*
    map->next_free_index ^= _handle->index;
    _handle->index ^= map->next_free_index;
    map->next_free_index ^= _handle->index;
    */

    map->next_free_index = _handle->index;

    /*
     * Zero out this entry
     * NOTE: this sets the index of the handle to KHNDL_INVALID_INDEX
     */
    destroy_khandle(_handle);

    mutex_unlock(map->lock);
    return 0;

error_and_unlock:
    mutex_unlock(map->lock);
    return -1;
}

/*!
 * @brief: Removes all instances of @addr of type @type
 *
 * Does not do any kind of object cleanup. Caller is responsable for that
 */
kerror_t khandle_map_remove(khandle_map_t* map, HANDLE_TYPE type, void* addr)
{
    if (!map || !map->lock)
        return -KERR_INVAL;

    mutex_lock(map->lock);

    for (u32 i = 0; i < map->max_count; i++) {
        if (map->handles[i].type != type || map->handles[i].reference.kobj != addr)
            continue;

        __destroy_khandle(&map->handles[i]);
    }

    mutex_unlock(map->lock);

    return 0;
}
