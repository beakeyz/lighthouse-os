/*
 * This file contains startup logic for app linked against the lightos library
 * LightOS acts as an extention to libc and provides the most basic platform functions
 * any app wishes to use. Our libc implementation uses lightos as a base for its
 * platform-specific implementations, but lightos is perfered for things like file-io
 * or memory allocations
 */
#include <mem/memory.h>

#include "lightos/api/handle.h"
#include <lightos/error.h>
#include <lightos/handle.h>
#include <lightos/lightos.h>
#include <stdlib.h>
#include <sys/types.h>

/* Generic process entry. No BS */
typedef int (*MainEntry)();
/* Process wants to have a handle to itself */
typedef int (*MainEntry_ex)(HANDLE this);
/* Process is run as a unix/POSIX thingy */
typedef int (*MainEntry_unix)(int argc, char* argv[]);

/* Define the thing here */
void lightapp_startup(lightos_appctx_t* ctx) __attribute__((used));

extern void __attribute__((noreturn)) halt(void);
extern void __init_memalloc(void);
extern void __init_stdio(void);

/*!
 * @brief: Get everything up and running for a standard C application under LightOS
 */
void __init_libc(void)
{
    /* 1) Init userspace libraries */
    /* 1.1 -> init heap for this process (e.g. Ask the kernel for some memory and dump our allocator there) */
    __init_memalloc();
    /* 1.2 -> init posix standard data streams (stdin, stdout, stderr) */
    __init_stdio();
}

/*!
 * Binary entry for statically linked apps
 */
void lightapp_startup(lightos_appctx_t* ctx)
{
    f_libinit c_init;
    error_t error;

    if (!ctx || !ctx->entry)
        exit(69);

    /* Walk the dependencies list and initialize LIGHTENTRY functions */
    for (u32 i = 0; i < ctx->nr_libentries; i++) {
        c_init = ctx->init_vec[i];

        /* Frick */
        if (!c_init)
            continue;

        /* Call the init func */
        error = c_init(ctx->self);

        /* Fuck */
        if (error)
            exit(error);
    }

    /* 2) pass appropriate arguments to the program and run it */
    error = ctx->entry(ctx->self);

    /* TODO: Call exit functions */
    close_handle(ctx->self);

    /* 3) Notify the kernel that we have exited so we can be cleaned up */
    exit(error);

    /* 4) Yield */
    halt();
}
