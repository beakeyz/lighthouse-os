
#include "string.h"
#include "sys/types.h"

size_t strlen (const char* str) {
    size_t s = 0;
    while (str[s] != 0)
        s++;
    return s;
}

int strcmp (const char * str1, const char *str2) {
    while (*str1 == *str2 && (*str1))
    {
        str1++;
        str2++;
    }
    return *(const unsigned char *)str1 - *(const unsigned char *)str2;
}

char* strcpy (char* dest, const char* src) {
    size_t s = 0;
    while (src[s] != '\0') {
        dest[s] = src[s];
        s++;
    }
    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n)
{
  if (!n)
    return FALSE;
  
  const char *ss1 = (const char *)s1;
  const char *ss2 = (const char *)s2;
  for (size_t i = 0; i < n; i++)
  {
    if (ss1[i] != ss2[i])
    {
      return FALSE;
    }
  }
  return TRUE;
}

void *memcpy(void * restrict dest, const void * restrict src, size_t length) 
{
  uint8_t* d = (uint8_t*)dest;
  uint8_t* s = (uint8_t*)src;
  for (uint64_t i = 0; i < length; i++) {
    d[i] = s[i];
  }
  return d;
}

void * memset(void * dest, int c, size_t n) {
	size_t i = 0;
	for ( ; i < n; ++i ) {
		((char *)dest)[i] = (char)c;
	}
	return dest;
}

void *memchr(const void *s, int c, size_t n)
{
    const unsigned char *copy = (const unsigned char *)s;
    for (size_t i = 0; i < n; i++)
    {
        if (copy[i] == c)
        {
            return (void *)(copy + i);
        }
    }

    return nullptr;
}

//void *memmove(void *dest, const void *src, size_t n);
