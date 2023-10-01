#ifndef __ANIVA_STRING__
#define __ANIVA_STRING__
#include <libk/stddef.h>

size_t strlen(const char* str);

// 1 = 2 ?
int strcmp (const char* str1, const char* str2);
int strncmp(const char *s1, const char *s2, size_t n);
// and then there where two
char* strcpy (char* dest, const char* src);

char* strdup(const char* str);

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

#endif // !__STRING__
