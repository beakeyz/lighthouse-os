#include "string.h"
#include "kernel/dev/debug/serial.h"
#include <libk/stddef.h>

/*
 * FIXME: this whole file is a trainwreck, for now I'll leave it like this, but in the future
 * I'll have to make sure to structure these kinds of functions propperly, so there aren't any
 * arch specific functions scattered around the codebase =/
 */

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

// TODO: dis mofo is broken as fuck, fix it
// for now, make it x86 specific
// FIXME: refactor memcpy and memset so they match the archetecture the kernel is being built for
void *memcpy(void * restrict dest, const void * restrict src, size_t length)
{
    /*uint8_t* d = (void*) dest;
    const uint8_t* s = (const void*) src;
	for(size_t i = 0; i < length; i++){
        d[i] = s[i];
	}
    return dest;*/

    asm volatile("rep movsb"
	            : : "D"(dest), "S"(src), "c"(length)
	            : "flags", "memory");
	return dest;
}

// TODO: this is very confusing, it differs from the standard. find out what to do with it.
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

// TODO: this is x86 specific. I'll leave it here for now, but when we aventually target other arches, we'll have to refactor this =/
//void *memset(void *data, int value, size_t length)
//{
//    asm volatile ("cld; rep stosb"
//                  : "=c"((int){0})
//                  : "rdi"(data), "a"(value), "c"(length)
//                  : "flags", "memory", "rdi");
//    return data;
//}

void * memset(void * dest, int c, size_t n) {
	size_t i = 0;
	for ( ; i < n; ++i ) {
		((char *)dest)[i] = c;
	}
	return dest;
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

// funnies
// Credit goes to: https://github.com/dreamos82/Dreamos64/

// quite the buffer
static char to_str_buff[128 * 2];

string to_string(uint64_t val) {
    uint8_t size = 0;
    uint64_t size_test = val;
    while (size_test / 10 > 0) {
        size_test /= 10;
        size++;
    }

    uint8_t index = 0;
    
    while (val / 10 > 0) {
        uint8_t remain = val % 10;
        val /= 10;
        to_str_buff[size - index] = remain + '0';
        index++;
    }
    uint8_t remain = val % 10;
    to_str_buff[size - index] = remain + '0';
    to_str_buff[size + 1] = 0;
    return to_str_buff;
}
