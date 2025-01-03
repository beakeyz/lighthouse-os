#ifndef __ANIVA_MEMORY_BUFFER__
#define __ANIVA_MEMORY_BUFFER__

#include "mem/tracker/tracker.h"
#include "mem/zalloc/zalloc.h"
#include <libk/stddef.h>

/*
 * Aniva buffers
 *
 * Kernel-level memory management structure that we can use to standardise memory useage
 * Can be itteratively implemented
 *
 * Basic priciple:
 * Processes all over the system can 'use' and 'remove' handles from their scope, which exposes (or unexposes) the
 * underlying raw buffer object to the process. When there are no references left, the buffer can be safely removed.
 *
 * This also means userprocesses can safely hold references to kernel memory, since they only need the handle, which we
 * can also privatise, by sticking the handle inside the processes own handle map
 */
void init_aniva_buffers();

enum ANIVA_BUFFER_METHOD {
    ANIVA_BUFFER_METHOD_INVAL = 0,
    ANIVA_BUFFER_METHOD_MALLOC,
    ANIVA_BUFFER_METHOD_ZALLOC,
    ANIVA_BUFFER_METHOD_KZALLOC,
    ANIVA_BUFFER_METHOD_KMEM,
    ANIVA_BUFFER_METHOD_OBJECT,
};

typedef u64 buffer_handle_t;
typedef buffer_handle_t BUFFER_HANDLE;

typedef union __aniva_buffer_mem_attr {
    struct {
        pml_entry_t* page_map;
        page_tracker_t* tracker;
    } kmem;
    zone_allocator_t* zallocator;
} aniva_buffer_mem_attr_t;

/*
 * A unified struct to handle data exports.
 *
 * Usage:
 *
 * aniva_buffer_t buffer;
 * (void)some_call(some, param, &buffer);
 *
 * ... Do some actions on the buffer
 *
 * aniva_buffer_unref(&buffer);
 *
 * This ensures the memory is cleaned when there are no references to it anymore
 *
 * TODO: Fully implement
 */
typedef struct aniva_buffer {
    void* data;
    size_t len;

    /* Method of cleanup */
    enum ANIVA_BUFFER_METHOD method;
    /* Reference count to keep track of this buffers lifetime */
    i32 refc;

    /* Memory attributes */
    aniva_buffer_mem_attr_t mem_attr;

    /* Different cleanup methods */
    union {
        FuncPtr f_cleanup;
        void (*f_malloc_cleanup)(void* ptr);
        void (*f_zalloc_cleanup)(zone_allocator_t* zalloc, void* ptr);
        void (*f_kzalloc_cleanup)(void* ptr, size_t len);
        void (*f_kmem_cleanup)(pml_entry_t* map, page_tracker_t* tracker, void* ptr, size_t len);
        void (*f_object_cleanup)(void* object);
    } cleanup;
} aniva_buffer_t;

int aniva_buffer_export(buffer_handle_t* phandle, void* data, size_t dsize, enum ANIVA_BUFFER_METHOD method, aniva_buffer_mem_attr_t* mem_attr, FuncPtr f_cleanup);

int aniva_buffer_release(buffer_handle_t handle);
int aniva_buffer_unpack(buffer_handle_t handle, void** pdata, size_t* psize);

#endif // !__ANIVA_MEMORY_BUFFER__
