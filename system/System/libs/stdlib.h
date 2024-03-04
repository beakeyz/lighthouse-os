#ifndef __LIGHTENV_LIBC_STDLIB__
#define __LIGHTENV_LIBC_STDLIB__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void exit(uintptr_t result);

/* Non-posix: Exit the app, since it hit a library function that has not yet been implemented */
extern void exit_noimpl(const char* impl_name);

extern void abort(void);
extern int atexit(void (*)(void));

extern double strtod(const char *nptr, char **endptr);
extern float strtof(const char *nptr, char **endptr);
extern double atof(const char * nptr);
extern int atoi(const char*);
extern long atol(const char * nptr);
extern long long atoll(const char * nptr);
extern int32_t labs(long int j);
extern int32_t strtol(const char * s, char **endptr, int base);
extern size_t strtoll(const char *nptr, char **endptr, int base);
extern uint32_t strtoul(const char *nptr, char **endptr, int base);
extern size_t strtoull(const char *nptr, char **endptr, int base);

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
