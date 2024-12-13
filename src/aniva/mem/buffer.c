#include "buffer.h"
#include "libk/flow/error.h"
#include "libk/stddef.h"
#include "logging/log.h"
#include "mem/zalloc/zalloc.h"
#include <libk/data/hashmap.h>
#include <mem/kmem.h>

buffer_handle_t __cur_handle_nr;
hashmap_t* __handle_hashmap;
zone_allocator_t* __buffer_allocator;

/* Around 32768 spaces for buffer objects */
#define ANIVA_BUFFER_MAX_HANDLE_NR 0x8000
#define ANIVA_BUFFER_KEY_FMT "hndl_%lld"

/*!
 * @brief: Grab the correct cleanup method, based on the method type
 */
static inline FuncPtr __get_cleanup_method(enum ANIVA_BUFFER_METHOD method)
{
    switch (method) {
    case ANIVA_BUFFER_METHOD_MALLOC:
        return kfree;
    case ANIVA_BUFFER_METHOD_ZALLOC:
        return zfree;
    case ANIVA_BUFFER_METHOD_KZALLOC:
        return kzfree;
    case ANIVA_BUFFER_METHOD_KMEM:
        return (FuncPtr)kmem_dealloc;
    default:
        break;
    }

    return nullptr;
}

/*!
 * @brief: 'generate' a new buffer handle
 *
 * Maybe in the future we'll do actual generating?
 */
static inline buffer_handle_t __generate_buffer_handle()
{
    return __cur_handle_nr++;
}

/*!
 * @brief: Stores the buffer at the location of the handle
 *
 * @buffer: A stack-allocated aniva buffer
 */
static inline int __store_aniva_buffer(aniva_buffer_t* buffer, buffer_handle_t handle)
{
    char name_buffer[64] = { 0 };
    aniva_buffer_t* new_buffer;

    if (!buffer)
        return -KERR_INVAL;

    /* Format a string as a key for in the hashmap */
    sfmt_sz(name_buffer, sizeof(name_buffer), ANIVA_BUFFER_KEY_FMT, handle);

    /* Allocate a new buffer buffer xD */
    new_buffer = zalloc_fixed(__buffer_allocator);

    if (!new_buffer)
        return -KERR_NOMEM;

    memcpy(new_buffer, buffer, sizeof(*buffer));

    /* Put this shit in the hashmap */
    if (hashmap_put(__handle_hashmap, name_buffer, new_buffer)) {
        zfree_fixed(__buffer_allocator, new_buffer);
        return -KERR_RANGE;
    }

    return 0;
}

/*!
 * @brief: Loads a aniva buffer from the hashmap
 *
 * We export the pointer to the buffer, since we might want to change some fields inside the buffer itself, like
 * refcount. We need to be aware of the fact that we can't leak any references to aniva_buffers to outside this
 * subsystem, since that would be very bad xD
 */
static inline kerror_t __load_aniva_buffer(aniva_buffer_t** buffer, buffer_handle_t handle)
{
    char name_buffer[64] = { 0 };
    aniva_buffer_t* stored_buffer;

    if (!buffer)
        return -KERR_INVAL;

    /* Format a string as a key for in the hashmap */
    sfmt_sz(name_buffer, sizeof(name_buffer), ANIVA_BUFFER_KEY_FMT, handle);

    /* Put this shit in the hashmap */
    stored_buffer = hashmap_get(__handle_hashmap, name_buffer);

    /* Check if this shit even exists */
    if (!stored_buffer)
        return -KERR_NOT_FOUND;

    /* Export the pointer */
    *buffer = stored_buffer;

    return 0;
}

static inline kerror_t __remove_aniva_buffer(buffer_handle_t handle)
{
    char name_buffer[64] = { 0 };
    aniva_buffer_t* stored_buffer;

    /* Format a string as a key for in the hashmap */
    sfmt_sz(name_buffer, sizeof(name_buffer), ANIVA_BUFFER_KEY_FMT, handle);

    /* Put this shit in the hashmap */
    stored_buffer = hashmap_remove(__handle_hashmap, name_buffer);

    /* Check if this shit did even exists */
    if (!stored_buffer)
        return -KERR_NOT_FOUND;

    return 0;
}

int aniva_buffer_export(buffer_handle_t* phandle, void* data, size_t dsize, enum ANIVA_BUFFER_METHOD method, aniva_buffer_mem_attr_t* mem_attr, FuncPtr f_cleanup)
{
    /* Allocate a buffer object on the stack */
    aniva_buffer_t buffer;
    buffer_handle_t handle;
    FuncPtr __f_cleanup;

    if (!phandle || !data || !dsize || !method)
        return -KERR_INVAL;

    /* Initialize the handle */
    buffer = (aniva_buffer_t) {
        /* Internal data buffer */
        .data = data,
        .len = dsize,
        /* Memory attribute used by certain cleanup methods */
        .mem_attr = (mem_attr != nullptr) ? (*mem_attr) : ((aniva_buffer_mem_attr_t) { 0 }),
        /* The cleanup method itself */
        .method = method,
        /* Current reference count */
        .refc = 1,
        /* NOTE: Cleanup gets set later */
        .cleanup.f_cleanup = nullptr,
    };

    /* Try to get a cleanup method */
    __f_cleanup = __get_cleanup_method(method);

    /* If we could not find a cleanup method, use the one passed by the caller */
    if (!__f_cleanup)
        __f_cleanup = f_cleanup;

    /* Set the cleanup thing */
    buffer.cleanup.f_cleanup = __f_cleanup;

    /* Set the handle we'll use */
    handle = __generate_buffer_handle();

    /* Try to store the buffer */
    if (__store_aniva_buffer(&buffer, handle))
        return -KERR_RANGE;

    /* Export the handle */
    *phandle = handle;

    return 0;
}

/*!
 * @brief: Do actual cleanup of a buffer
 *
 * Removes the buffer from the hashmap and destroys the underlying buffer memory/object
 *
 * @buffer: Should be a pointer to the buffer that lives inside the hashmap
 * @handle: Handle handle used to find this buffer
 */
static inline kerror_t __cleanup_aniva_buffer(aniva_buffer_t* buffer, buffer_handle_t handle)
{
    /* Check if we have a cleanup method */
    if (buffer->cleanup.f_cleanup) {
        switch (buffer->method) {
        case ANIVA_BUFFER_METHOD_MALLOC:
            buffer->cleanup.f_malloc_cleanup(buffer->data);
            break;
        case ANIVA_BUFFER_METHOD_ZALLOC:
            buffer->cleanup.f_zalloc_cleanup(buffer->mem_attr.zallocator, buffer->data);
            break;
        case ANIVA_BUFFER_METHOD_KZALLOC:
            buffer->cleanup.f_kzalloc_cleanup(buffer->data, buffer->len);
            break;
        case ANIVA_BUFFER_METHOD_KMEM:
            buffer->cleanup.f_kmem_cleanup(buffer->mem_attr.kmem.page_map, buffer->mem_attr.kmem.resource_bundle, buffer->data, buffer->len);
            break;
        case ANIVA_BUFFER_METHOD_OBJECT:
            buffer->cleanup.f_object_cleanup(buffer->data);
            break;
        default:
            break;
        }
    }

    /* Try to remove the handle from the hashmap */
    if (__remove_aniva_buffer(handle))
        return -KERR_NOT_FOUND;

    /* Claim back the memory used for this buffer */
    zfree_fixed(__buffer_allocator, buffer);

    return 0;
}

/*!
 * @brief: Releases a reference to a certain aniva buffer
 *
 * This is called when a unpacked buffer is done being used.
 */
int aniva_buffer_release(buffer_handle_t handle)
{
    aniva_buffer_t* buffer = NULL;

    /* Try to find this shit */
    if (__load_aniva_buffer(&buffer, handle) || !buffer)
        return -KERR_NOT_FOUND;

    /* Reduce the refcount */
    buffer->refc--;

    /* This buffer still has references, don't do anything */
    if (buffer->refc > 0)
        return 0;

    return __cleanup_aniva_buffer(buffer, handle);
}

/*!
 * @brief: Unpacks the data from a buffer
 *
 * This adds a reference to the buffer and then gives the caller access to the underlying data
 * of the buffer. As long as reference and release calls are correctly ordered, we should not
 * have preemptive destruction of memory objects that are still in use =0
 *
 * NOTE: Caller can put @pdata or @psize to NULL if they are not interested in the buffer contents
 * and just want to leak a reference.
 */
int aniva_buffer_unpack(buffer_handle_t handle, void** pdata, size_t* psize)
{
    aniva_buffer_t* buffer;

    /* Set this to null just in case */
    buffer = NULL;

    /* Try to find the buffer */
    if (__load_aniva_buffer(&buffer, handle) || !buffer)
        return -KERR_NOT_FOUND;

    /* Add a reference */
    buffer->refc++;

    /* Export the buffers contents, if the caller wants them */
    if (pdata)
        *pdata = buffer->data;

    if (psize)
        *psize = buffer->len;

    return 0;
}

/*!
 * @brief: Initialize the anvia buffers subsystem
 *
 * We need a global list for keeping all the buffers
 */
void init_aniva_buffers()
{
    __cur_handle_nr = 0;
    __handle_hashmap = create_hashmap(ANIVA_BUFFER_MAX_HANDLE_NR, NULL);
    __buffer_allocator = create_zone_allocator(64 * Kib, sizeof(aniva_buffer_t), NULL);
}
