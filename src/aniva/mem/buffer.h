#ifndef __ANIVA_MEMORY_BUFFER__
#define __ANIVA_MEMORY_BUFFER__

#include "mem/zalloc/zalloc.h"
#include "system/resource.h"
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
    /*  */
    u32 refc;

    /* Different cleanup methods */
    union {
        void (*f_malloc_cleanup)(void* ptr);
        void (*f_zalloc_cleanup)(zone_allocator_t* zalloc, void* ptr);
        void (*f_kzalloc_cleanup)(void* ptr, size_t len);
        void (*f_kmem_cleanup)(pml_entry_t* map, kresource_bundle_t* resources, void* ptr, size_t len);
        void (*f_object_cleanup)(void* object);
    } cleanup;
} aniva_buffer_t;

void aniva_buffer_export(aniva_buffer_t* buffer, void* data, size_t len, enum ANIVA_BUFFER_METHOD method);

int aniva_buffer_unref(aniva_buffer_t* buffer);
int aniva_buffer_ref(aniva_buffer_t* buffer);

#endif // !__ANIVA_MEMORY_BUFFER__
