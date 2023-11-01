#ifndef __LIGNTENV_LIBC_STRING__
#define __LIGNTENV_LIBC_STRING__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

extern unsigned long long strlen(const char* str);

extern int strcmp (const char* str1, const char* str2);
extern char* strcpy (char* dest, const char* src);

extern int memcmp (const void* dest, const void* src, size_t size);
extern void *memcpy(void * restrict dest, const void * restrict src, size_t length);

extern void *memmove(void *dest, const void *src, size_t n);
extern void *memset(void *data, int value, size_t length);
extern void *memchr(const void *s, int c, size_t n);

#ifdef __cplusplus
}
#endif

#endif // !__LIGNTENV_LIBC_STRING__
