#include "string.h"
#include <stddef.h>

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

_string strcpy (_string dest, string src) {
    size_t s = 0;
    while (src[s] != '\0') {
        dest[s] = src[s];
        s++;
    }
    return dest;
}

void *memcpy(void *dest, const void *src, size_t length)
{
    char *cdest = (char *)dest;
    const char *csrc = (const char *)src;
    for (size_t i = 0; i < length; i++)
    {
        cdest[i] = csrc[i];
    }
    return cdest;
}

int memcmp(const void *s1, const void *s2, size_t n)
{
    const char *ss1 = (const char *)s1;
    const char *ss2 = (const char *)s2;
    for (size_t i = 0; i < n; i++)
    {
        if (ss1[i] != ss2[i])
        {
            return false;
        }
    }
    return true;
}
void *memset(void *data, int value, size_t length)
{
    uint8_t *d = (uint8_t *)data;
    for (size_t i = 0; i < length; i++)
    {
        d[i] = value;
    }
    return data;
}

/*
void *memmove(void *dest, const void *src, size_t n)
{
    char *new_dst = (char *)dest;
    const char *new_src = (const char *)src;
    char *temporary_data = (char *)malloc(n);

    for (size_t i = 0; i < n; i++)
    {
        temporary_data[i] = new_src[i];
    }
    for (size_t i = 0; i < n; i++)
    {
        new_dst[i] = temporary_data[i];
    }

    free(temporary_data);
    return dest;
}
*/

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
