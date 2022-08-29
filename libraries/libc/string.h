#ifndef __STRING__
#define __STRING__
#include <libc/stddef.h>

#define string const char*
#define _string char*

size_t strlen(string str);

// 1 = 2 ?
int strcmp (string str1, string str2);
// and then there where two
_string strcpy (_string dest, string src);

// mem shit

int memcmp (const void* dest, const void* src, size_t size);
void *memcpy(void *dest, const void *src, size_t length);
// Problematic rn
//void *memmove(void *dest, const void *src, size_t n);
void *memset(void *data, int value, size_t length);
void *memchr(const void *s, int c, size_t n);


#endif // !__STRING__
