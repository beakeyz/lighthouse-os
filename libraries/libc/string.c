#include "string.h"
#include <libc/stddef.h>

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
void *memcpy(void * restrict dest, const void * restrict src, size_t length)
{

	for(size_t i = 0; i < length; i++){
	}
    return dest;
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
    asm volatile ("cld; rep stosb"
                  : "=c"((int){0})
                  : "rdi"(data), "a"(value), "c"(length)
                  : "flags", "memory", "rdi");
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

// funnies
// Credit goes to: https://github.com/dreamos82/Dreamos64/

int string_from_dec(char *buff, uint32_t n) {
    if (n < 0) {
        *buff++ = '-';
        n = ((uint32_t) n * -1);
    }

    char *pointer, *pointerbase;
    int mod;
    int size = 0;
    pointer = buff;
    pointerbase = buff;
    if(n == 0) {
        *pointer = '0';
        return 1;
    }
    while (n > 0) {
        mod = n % 10;
        *pointer++ = mod + '0';
        n = n / 10;
        size++;
    }
    *pointer--=0;
    while(pointer > pointerbase){
        char swap;
        swap = *pointer;
        *pointer = *pointerbase;
        *pointerbase = swap;
        pointerbase++;
        pointer--;
    }

    return size;
}

int string_from_hex(char *buff, uint32_t n) {
    unsigned long tmpnumber = n;
    int shift = 0;
    int size = 0;
    char *hexstring = buff;
    while (tmpnumber >= 16){
        tmpnumber >>= 4;
        shift++;
    }
    size = shift + 1;
    /* Now i can print the digits masking every single digit with 0xF 
     * Each hex digit is 4 bytes long. So if i mask number&0xF
     * I obtain exactly the number identified by the digit
     * i.e. number is 0xA3 0XA3&0xF=3  
     **/
    char hex_base = 'A';

    for(; shift >=0; shift--){
        tmpnumber = n;
        tmpnumber>>=(4*shift);
        tmpnumber&=0xF;

        if(tmpnumber < 10){
            *hexstring++ = '0' + tmpnumber; //Same as decimal number
        } else {
            *hexstring++ = hex_base + tmpnumber-10; //11-15 Are letters
        }
        *hexstring = '\0';
    }
    return size;
}
