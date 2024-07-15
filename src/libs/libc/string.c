
#include "string.h"
#include "sys/types.h"
#include <stdlib.h>

size_t strlen(const char* str)
{
    size_t s = 0;
    while (str[s] != 0)
        s++;
    return s;
}

int strcmp(const char* str1, const char* str2)
{
    while (*str1 == *str2) {
        if (*str1 == NULL)
            return 0;

        str1++;
        str2++;
    }

    return *(const unsigned char*)str1 - *(const unsigned char*)str2;
}

int strncmp(const char* s1, const char* s2, uint32_t len)
{
    if (len == 0)
        return 0;

    while (len-- && *s1 == *s2) {
        if (!len || !*s1)
            break;
        s1++;
        s2++;
    }

    return (*(unsigned char*)s1) - (*(unsigned char*)s2);
}

/*
 * Find a character inside a memory buffer
 */
void* memrchr(const void* m, int c, size_t n)
{
    const unsigned char* s = m;
    c = (unsigned char)c;
    while (n--) {
        if (s[n] == c) {
            return (void*)(s + n);
        }
    }
    return 0;
}

/* ???
 */
char* strchr(const char* str, char chr)
{
    return memrchr(str, chr, strlen(str) + 1);
}

/* ???
 */
char* strrchr(const char* str, char chr)
{
    return memrchr(str, chr, strlen(str) + 1);
}

char* strdup(const char* str)
{
    char* ret;
    size_t len;

    len = strlen(str) + 1;

    if (!len)
        return nullptr;

    ret = malloc(len);

    if (!ret)
        return nullptr;

    memset(ret, 0, len);
    strcpy(ret, str);
    return ret;
}

char* strstr(const char* str1, const char* str2)
{
    uint32_t cur_search_len;
    uint32_t cur_search_idx;
    uint32_t needle_search_idx;
    uint32_t needle_len;

    if (str2[0] == NULL)
        return (char*)str1;

    cur_search_idx = 0;
    needle_len = strlen(str2);

    while (str1[cur_search_idx]) {
        cur_search_len = 1;
        needle_search_idx = 0;

        while (str1[cur_search_idx] == str2[needle_search_idx]) {

            /* Return the start of the thing we found */
            if (cur_search_len == needle_len)
                return (char*)(str1 + cur_search_idx - needle_search_idx);

            needle_search_idx++;
            cur_search_len++;
        }

        cur_search_idx += cur_search_len;
    }

    return nullptr;
}

char* strcpy(char* dest, const char* src)
{
    size_t s = 0;
    while (src[s] != '\0') {
        dest[s] = src[s];
        s++;
    }
    return dest;
}

char* strncpy(char* dest, const char* src, uint32_t len)
{
    char* out = dest;

    while (len > 0) {
        *out = (*src != NULL) ? *src : '\0';
        ++out;
        ++src;
        --len;
    }

    return out;
}

int memcmp(const void* s1, const void* s2, size_t n)
{
    if (!n)
        return FALSE;

    const char* ss1 = (const char*)s1;
    const char* ss2 = (const char*)s2;
    for (size_t i = 0; i < n; i++) {
        if (ss1[i] != ss2[i]) {
            return FALSE;
        }
    }
    return TRUE;
}

void* memcpy(void* restrict dest, const void* restrict src, size_t length)
{
    uint8_t* d = (uint8_t*)dest;
    uint8_t* s = (uint8_t*)src;
    for (uint64_t i = 0; i < length; i++) {
        d[i] = s[i];
    }
    return d;
}

void* memset(void* dest, int c, size_t n)
{
    size_t i = 0;
    for (; i < n; ++i) {
        ((char*)dest)[i] = (char)c;
    }
    return dest;
}

void* memchr(const void* s, int c, size_t n)
{
    const unsigned char* copy = (const unsigned char*)s;
    for (size_t i = 0; i < n; i++) {
        if (copy[i] == c) {
            return (void*)(copy + i);
        }
    }

    return nullptr;
}

static inline bool _isxdigit(char c)
{
    return ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'));
}

static const char* __parse_base(const char* s, int* base)
{
    if (!*base) {
        if (s[0] != '0')
            *base = 10;
        else {
            if ((s[1] == 'x' || s[1] == 'X') && _isxdigit(s[2]))
                *base = 16;
            else
                *base = 8;
        }
    }

    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X') && _isxdigit(s[2]))
        s += 2;

    return s;
}

/*!
 * @brief: Quick and dirty strtoul (strtoull)
 */
size_t strtoull(const char* cp, char** endptr, int base)
{
    uint64_t result = 0ULL;

    cp = __parse_base(cp, &base);

    /* Check for numbers */
    while (*cp && _isxdigit(*cp)) {

        uint32_t c_val;

        if (*cp >= '0' && *cp <= '9')
            c_val = *cp - '0';
        else if ((*cp >= 'a' && *cp <= 'f'))
            c_val = *cp - 'a' + 10;
        else if ((*cp >= 'A' && *cp <= 'F'))
            c_val = *cp - 'A' + 10;
        else
            break;

        if (c_val > base)
            break;

        result = result * base + c_val;
        cp++;
    }

    if (endptr)
        *endptr = (char*)cp;

    return result;
}
