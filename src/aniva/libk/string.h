#ifndef __ANIVA_STRING__
#define __ANIVA_STRING__
#include <libk/stddef.h>

size_t strlen(const char* str);

// 1 = 2 ?
int strcmp (const char* str1, const char* str2);
// and then there where two
char* strcpy (char* dest, const char* src);

// mem shit

int memcmp (const void* dest, const void* src, size_t size);
void *memcpy(void * restrict dest, const void * restrict src, size_t length);
// Problematic rn
void *memmove(void *dest, const void *src, size_t n);
void *memset(void *data, int value, size_t length);
void *memchr(const void *s, int c, size_t n);

const char* concat(char* one, char* two);

// different kinds of number-to-string formating

const char* to_string (uint64_t val);

#endif // !__STRING__
