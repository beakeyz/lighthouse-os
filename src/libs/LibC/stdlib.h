#ifndef __LIGHTENV_LIBC_STDLIB__
#define __LIGHTENV_LIBC_STDLIB__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void exit(uintptr_t result);

extern void abort(void);
extern int atexit(void (*)(void));
extern int atoi(const char*);

/*
 * Allocate a block of memory on the heap
 */
extern void* malloc(size_t __size) __attribute__((malloc));

/*
 * Allocate count blocks of memory of __size bytes
 */
extern void* calloc(size_t count, size_t __size) __attribute__((malloc));

/*
 * Reallocate the block at ptr of __size bytes
 */
extern void* realloc(void* ptr, size_t __size) __attribute__((malloc));

/*
 * Free a block of memory allocated by malloc, calloc or realloc
 */
extern void free(void*);

extern char* getenv(const char*);

extern int system(const char* cmd);

#ifdef __cplusplus
}
#endif

#endif // !__LIGHTENV_LIBC_STDLIB__
