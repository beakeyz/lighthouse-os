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

extern void* malloc(size_t __size);
extern void* calloc(size_t count, size_t __size);

extern void* realloc(void* ptr, size_t __size);
extern void free(void*);

extern char* getenv(const char*);

extern int system(const char* cmd);

#ifdef __cplusplus
}
#endif

#endif // !__LIGHTENV_LIBC_STDLIB__
