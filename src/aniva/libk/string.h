#ifndef __ANIVA_STRING__
#define __ANIVA_STRING__

#include <libk/stddef.h>

size_t strlen(const char* str);

// 1 = 2 ?
int strcmp (const char* str1, const char* str2);
int strncmp(const char *s1, const char *s2, size_t n);
// and then there where two
char* strcpy (char* dest, const char* src);
char* strncpy (char* dest, const char* src, size_t len);
char* strdup(const char* str);
char* strcat (char*, const char*);
char* strncat (char*, const char*, size_t);

char *strstr(const char * h, const char * n);

// mem shit

bool memcmp (const void* dest, const void* src, size_t size);
void *memcpy(void * restrict dest, const void * restrict src, size_t length);
// Problematic rn
void *memmove(void *dest, const void *src, size_t n);
void *memset(void *data, int value, size_t length);
void *memchr(const void *s, int c, size_t n);

const int concat(char* one, char* two, char* out);

// different kinds of number-to-string formating

const char* to_string (uint64_t val);

uint64_t dirty_strtoul(const char *cp, char **endp, unsigned int base);

#endif // !__STRING__
