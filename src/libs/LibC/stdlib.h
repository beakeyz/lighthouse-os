#ifndef __LIGHTENV_LIBC_STDLIB__
#define __LIGHTENV_LIBC_STDLIB__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void exit(uintptr_t result);

void abort(void);
int atexit(void (*)(void));
int atoi(const char*);
void free(void*);
void* malloc(unsigned long long);
char* getenv(const char*);


#ifdef __cplusplus
}
#endif

#endif // !__LIGHTENV_LIBC_STDLIB__
