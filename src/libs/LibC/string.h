#ifndef __LIGNTENV_LIBC_STRING__
#define __LIGNTENV_LIBC_STRING__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

extern unsigned long long strlen(const char* str);

extern int strcmp (const char* str1, const char* str2);
extern int strncmp (const char* str1, const char* str2, uint32_t len);
extern char* strcpy (char* dest, const char* src);
extern char* strncpy (char* dest, const char* src, uint32_t len);
extern char* strrchr(const char* str, char chr);
extern char* strdup(const char* str);
extern char* strstr(const char* str1, const char* str2);

extern double strtod(const char *nptr, char **endptr);
extern float strtof(const char *nptr, char **endptr);
extern double atof(const char * nptr);
extern int atoi(const char * nptr);
extern long atol(const char * nptr);
extern int32_t labs(long int j);
extern int32_t strtol(const char * s, char **endptr, int base);
extern ssize_t strtoll(const char *nptr, char **endptr, int base);
extern uint32_t strtoul(const char *nptr, char **endptr, int base);
extern size_t strtoull(const char *nptr, char **endptr, int base);

extern int memcmp (const void* dest, const void* src, size_t size);
extern void *memcpy(void * restrict dest, const void * restrict src, size_t length);

extern void *memmove(void *dest, const void *src, size_t n);
extern void *memset(void *data, int value, size_t length);
extern void *memchr(const void *s, int c, size_t n);

#ifdef __cplusplus
}
#endif

#endif // !__LIGNTENV_LIBC_STRING__
