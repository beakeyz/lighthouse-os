#ifndef __LIGHTOS_UNILIB_H__
#define __LIGHTOS_UNILIB_H__

#include <stdint.h>

struct unilib_file;

/*
 * Unilib initialization block
 *
 * This struct gets passed to the united library by the runtime. It contains all the information needed
 * by the unilib to run
 *
 * TODO: Create libraries using unilib
 * TODO: Support unilib in both the kernel and userspace
 */
typedef struct unilib_init_block {
    void* (*f_mem_alloc)(size_t nr_bytes);
    void (*f_mem_dealloc)(void* data);

    struct unilib_file* (*f_open_file)(const char* path, const char* mode);
    int (*f_close_file)(struct unilib_file* file);
} unilib_init_block_t;

#endif // !__LIGHTOS_UNILIB_H__
