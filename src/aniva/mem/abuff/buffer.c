#include "buffer.h"
#include "libk/data/bitmap.h"
#include "libk/flow/error.h"
#include "mem/heap.h"
#include "mem/zalloc/zalloc.h"
#include <libk/data/hashmap.h>
#include <mem/kmem.h>

struct abuffer_register;

static zone_allocator_t* __buffer_allocator;
static struct abuffer_register* __registry;

/* Around 32768 spaces for buffer objects */
#define ANIVA_BUFFER_MAX_HANDLE_NR 0x8000

/*
 * A single buffer register for aniva buffers
 *
 * These registers are responsible for ensuring user-accessible objects are kept alive until
 * there are no references anymore. When exporting an object to the abuffer registry, memory
 * ownership is essentially transferred to the registry code. Any subsequent 'destroy' calls
 * on the exported object(s) by the subsystem are considered illegal behaviour.
 *
 * These registers should be able to:
 * 1) Generate abuffer handles for any aniva buffer that needs to be exported
 * 2) Free unreferenced buffers automatically
 */
typedef struct abuffer_register {
    /* Dword to specify the capacity this register has */
    u32 dw_capacity;
    /* Dword to tell us how many entries are used in this register */
    u32 dw_nr_used;
    /* Bitmap to keep track of which buffer slots are used */
    bitmap_t* freemap;
    /* Pointer to the (potential) next register */
    struct abuffer_register* next;
    /* Array of buffer pointers, which are allocated in their own object pool */
    abuffer_t* buffer_list[];
} abuffer_register_t;

/*!
 * @brief: Initialize a buffer register
 */
static void __init_abuffer_register(abuffer_register_t** p_ret, u32 capacity)
{
    size_t reg_sz;
    size_t aligned_reg_sz;
    abuffer_register_t* ret;

    if (!p_ret || !capacity)
        return;

    /* Ensure we have a page size */
    reg_sz = sizeof(abuffer_register_t) + (capacity * sizeof(abuffer_t*));
    /* Calculate the aligned size */
    aligned_reg_sz = ALIGN_UP_TO_PAGE(reg_sz);
    /* Add the delta to the capacity */
    capacity += ((aligned_reg_sz - reg_sz) >> 3);

    if (kmem_kernel_alloc_range((void**)&ret, aligned_reg_sz, NULL, KMEM_FLAG_WRITABLE))
        return;

    /* Clear out the region */
    memset(ret, 0, aligned_reg_sz);

    /* Set the fields */
    ret->dw_capacity = capacity;
    /*
     * Allocate a bitmap
     * TODO: Figure out if this isn't too memory intensive
     */
    ret->freemap = create_bitmap_ex(capacity, 0);

    /* Export the fucker */
    *p_ret = ret;
}

/*!
 * @brief: Destroy a buffer register
 *
 * TODO: Verify that the containing buffers are ready for killing
 */
static void __destroy_abuffer_register(abuffer_register_t** reg)
{
    abuffer_register_t* o_reg;

    if (!reg || !(*reg))
        return;

    /* Dereference this shit */
    o_reg = *reg;

    /* Kill the freemap */
    destroy_bitmap(o_reg->freemap);

    /* Ensure this bastard doesn't fuck up our link */
    *reg = o_reg->next;

    /* Deallocate the kernel buffer */
    ASSERT(IS_OK(kmem_kernel_dealloc((vaddr_t)o_reg, sizeof(abuffer_register_t) + (o_reg->dw_capacity * sizeof(abuffer_t*)))));
}

static error_t __abuffer_reg_put(abuffer_register_t* reg, abuffer_t* buff, abuffer_handle_t handle)
{
    u64 key = abuffer_hndl_get_key(handle);

    /* Yikes */
    if (key >= reg->dw_capacity)
        return -ERANGE;

    /* Mark this entry */
    bitmap_mark(reg->freemap, key);

    /* Place the buffer in the list */
    reg->buffer_list[key] = buff;

    return 0;
}

static error_t __abuffer_reg_get(abuffer_register_t* reg, abuffer_handle_t handle, abuffer_t** pbuff)
{
    u64 key;

    /* Check if we aren't ded */
    if (!reg)
        return -EINVAL;

    /* Grab the key */
    key = abuffer_hndl_get_key(handle);

    if (key >= reg->dw_capacity)
        return -ERANGE;

    /* Export the pointer (dangerous lol) */
    *pbuff = reg->buffer_list[key];

    return 0;
}

static error_t __abuffer_reg_remove(abuffer_register_t* reg, abuffer_handle_t handle, abuffer_t** pbuff)
{
    u64 key;

    /* Check if we aren't ded */
    if (!reg)
        return -EINVAL;

    /* Grab the key */ key = abuffer_hndl_get_key(handle);

    if (key >= reg->dw_capacity)
        return -ERANGE;

    /* Mark the thing as free */
    bitmap_unmark(reg->freemap, key);

    /* Export the pointer (dangerous lol) */
    *pbuff = reg->buffer_list[key];
    /* Clear this entry just in case */
    reg->buffer_list[key] = nullptr;

    return 0;
}

/*!
 * @brief: Register a buffer loll
 */
static error_t __abuffer_register(abuffer_t* buffer, abuffer_handle_t* phandle)
{
    abuffer_handle_t reg_idx = 0;
    abuffer_handle_t handle = 0;
    abuffer_register_t* reg = __registry;

    /* Check if our params are correct */
    if (!buffer || !phandle)
        return -EINVAL;

    /* Walk the current link of registers */
    while (reg) {
        if (IS_OK((bitmap_find_free(reg->freemap, &handle))))
            break;

        /* Couldn't find a free place, go next */
        reg_idx++;
        reg = reg->next;
    }

    if (!reg)
        return -ENOSPC;

    /* Set the handle */
    *phandle = abuffer_hndl_set_reg(handle, reg_idx);

    /* Put the buffer in the register */
    return __abuffer_reg_put(reg, buffer, handle);
}

static error_t __abuffer_get(abuffer_handle_t handle, abuffer_t** pbuff)
{
    u64 reg = abuffer_hndl_get_reg(handle);
    abuffer_register_t* p_reg = __registry;

    /* Walk the list until we find our bastard */
    while (reg-- && p_reg)
        p_reg = p_reg->next;

    /* Get from this register */
    return __abuffer_reg_get(p_reg, handle, pbuff);
}

static error_t __abuffer_remove(abuffer_handle_t handle, abuffer_t** pbuff)
{
    u64 reg = abuffer_hndl_get_reg(handle);
    abuffer_register_t* p_reg = __registry;

    /* Walk the list until we find our bastard */
    while (reg-- && p_reg)
        p_reg = p_reg->next;

    /* Get from this register */
    return __abuffer_reg_remove(p_reg, handle, pbuff);
}

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
 * @brief: Stores the buffer at the location of the handle
 *
 * @buffer: A stack-allocated aniva buffer
 */
static inline int __store_aniva_buffer(aniva_buffer_t* buffer, abuffer_handle_t* handle)
{
    aniva_buffer_t* new_buffer;

    if (!buffer)
        return -KERR_INVAL;

    /* Allocate a new buffer buffer xD */
    new_buffer = zalloc_fixed(__buffer_allocator);

    if (!new_buffer)
        return -KERR_NOMEM;

    memcpy(new_buffer, buffer, sizeof(*buffer));

    return __abuffer_register(new_buffer, handle);
}

/*!
 * @brief: Loads a aniva buffer from the hashmap
 *
 * We export the pointer to the buffer, since we might want to change some fields inside the buffer itself, like
 * refcount. We need to be aware of the fact that we can't leak any references to aniva_buffers to outside this
 * subsystem, since that would be very bad xD
 */
static inline kerror_t __load_aniva_buffer(aniva_buffer_t** buffer, abuffer_handle_t handle)
{
    aniva_buffer_t* stored_buffer = NULL;

    if (!buffer)
        return -KERR_INVAL;

    /* Check if this shit even exists */
    if (__abuffer_get(handle, &stored_buffer) || !stored_buffer)
        return -ENOENT;

    /* Export the pointer */
    *buffer = stored_buffer;

    return 0;
}

/*!
 * @brief: Remove a buffer from the registry
 *
 * NOTE: This also kills the allocation
 */
static inline kerror_t __remove_aniva_buffer(abuffer_handle_t handle)
{
    aniva_buffer_t* stored_buffer = NULL;

    if (__abuffer_remove(handle, &stored_buffer) || !stored_buffer)
        return -ENOENT;

    /* Claim back the memory used for this buffer */
    zfree_fixed(__buffer_allocator, stored_buffer);
    return 0;
}

/*!
 * @brief: Export an object to the aniva buffer registry
 */
int aniva_buffer_export(abuffer_handle_t* phandle, void* data, size_t dsize, enum ANIVA_BUFFER_METHOD method, aniva_buffer_mem_attr_t* mem_attr, FuncPtr f_cleanup)
{
    /* Allocate a buffer object on the stack */
    aniva_buffer_t buffer;
    abuffer_handle_t handle;
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

    /* Try to store the buffer */
    if (__store_aniva_buffer(&buffer, &handle))
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
static inline kerror_t __cleanup_aniva_buffer(aniva_buffer_t* buffer, abuffer_handle_t handle)
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
            buffer->cleanup.f_kmem_cleanup(buffer->mem_attr.kmem.page_map, buffer->mem_attr.kmem.tracker, buffer->data, buffer->len);
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

    return 0;
}

/*!
 * @brief: Releases a reference to a certain aniva buffer
 *
 * This is called when a unpacked buffer is done being used.
 */
int aniva_buffer_release(abuffer_handle_t handle)
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
 * @brief: Tries to see if we can destroy a given buffer
 */
int aniva_buffer_try_destroy(abuffer_handle_t handle)
{
    aniva_buffer_t* buffer = NULL;

    /* Try to find this shit */
    if (__load_aniva_buffer(&buffer, handle) || !buffer)
        return -KERR_NOT_FOUND;

    /* Check if this guy has external references */
    if (buffer->refc > 1)
        return -ETOOMANYREFS;

    /*
     * It doesn't! We can kill it >:)
     * NOTE: Every buffer has at least one reference which is held by the register. This guy
     * obviously doesn't care about this reference, since we just keep this 'reference' for
     * convineance, since we can do unrefs way easier.
     */
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
int aniva_buffer_unpack(abuffer_handle_t handle, void** pdata, size_t* psize)
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
    /* Initialize an initial registry */
    __init_abuffer_register(&__registry, ANIVA_BUFFER_MAX_HANDLE_NR);

    /* Initialize the object pool for the buffers */
    __buffer_allocator = create_zone_allocator(64 * Kib, sizeof(aniva_buffer_t), NULL);
}
