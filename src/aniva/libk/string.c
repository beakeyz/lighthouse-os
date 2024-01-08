#include "string.h"
#include "dev/debug/serial.h"
#include "libk/flow/error.h"
#include "logging/log.h"
#include "mem/heap.h"
#include <libk/stddef.h>

/*
 * FIXME: this whole file is a trainwreck, for now I'll leave it like this, but in the future
 * I'll have to make sure to structure these kinds of functions propperly, so there aren't any
 * arch specific functions scattered around the codebase =/
 */

size_t strlen(const char* str) 
{
  size_t s = 0;
  while (str[s] != 0)
      s++;
  return s;
}

int strcmp(const char * str1, const char *str2) 
{
  while (*str1 == *str2) 
  {
    if (*str1 == NULL)
      return 0;

    str1++;
    str2++;
  }

  return *(const unsigned char *)str1 - *(const unsigned char *)str2;
}

int strncmp(const char *s1, const char *s2, size_t n) 
{
  char c1, c2;

  for (size_t i = 0; i < n; i++) {
    c1 = s1[i];
    c2 = s2[i];

    if (c1 != c2)
      return c1 < c2 ? -1 : 1;
    if (!c1)
      return 0;
  }

  return 0;
}

char* strcpy (char* dest, const char* src) 
{
    size_t s = 0;
    while (src[s]) {
      dest[s] = src[s];
      s++;
    }
    return dest;
}

char* strncpy (char* dest, const char* src, size_t len)
{
  char * out = dest;

  while (len > 0) {
    *out = (*src != NULL) ?
      *src :
      '\0';
    ++out;
    ++src;
    --len;
  }

  return out;
}

char* strdup(const char* str)
{
  char* ret;

  if (!str)
    return nullptr;

  size_t len = strlen(str);

  ret = kmalloc(len + 1);

  ASSERT_MSG(ret, "Failed to allocate a new string for kernel_strdup");

  memset(ret, 0, len + 1);
  strcpy(ret, str);

  return ret;
}

char* strcat (char* dest, const char* s)
{
  char * end = dest;

  while (*end != '\0')
    ++end;

  while (*s) {
    *end = *s;
    end++;
    s++;
  }

  *end = '\0';

  return dest;
}

char* strncat (char* dest, const char* src, size_t len)
{
  char * end = dest;

  while (*end != '\0')
    ++end;

  size_t i = 0;

  while (*src && i < len) {
    *end = *src;
    end++;
    src++;
    i++;
  }

  *end = '\0';
  return dest;
}

char *strstr(const char * h, const char * n)
{
  kernel_panic("TODO: strstr");
}

// TODO: dis mofo is broken as fuck, fix it
// for now, make it x86 specific
// FIXME: refactor memcpy and memset so they match the arch the kernel is being built for
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
bool memcmp(const void *s1, const void *s2, size_t n)
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

/*!
 * @brief: Set a block of memory to a uniform value
 *
 * TODO: optimize
 */
void * memset(void * dest, int c, size_t n) {
	size_t i = 0;
	for ( ; i < n; ++i ) {
		((char *)dest)[i] = (char)c;
	}
	return dest;
}

void *memmove(void *dest, const void *src, size_t n)
{
  kernel_panic("(memmove) Just don't");
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

const int concat(char* one, char* two, char* out) {
  if (out == nullptr) {
    return -1;
  }

  const size_t total_length = strlen(one) + strlen(two) + 1;
  // TODO this sux
  memset(out, 0, total_length);
  memcpy(out, one, strlen(one));
  memcpy(out + strlen(one), two, strlen(two));
  out[total_length-1] = '\0';

  return 0;
}

// funnies
// Credit goes to: https://github.com/dreamos82/Dreamos64/

// quite the buffer
static char to_str_buff[128 * 2];

const char* to_string(uint64_t val) {
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

static inline bool _isxdigit(char c)
{
  return ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'));
}

static const char* parse_base(const char* s, uint32_t* base)
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
    s+=2;

  return s;
}

/*!
 * @brief: Quick and dirty strtoul (strtoull)
 */
uint64_t dirty_strtoul(const char *cp, char **endp, unsigned int base)
{
  uint64_t result = 0ULL;

  cp = parse_base(cp, &base);

  /* Check for numbers */
  while (*cp && _isxdigit(*cp)) {

    uint32_t c_val;

    if (*cp >= '0' && *cp <= '9')
      c_val = *cp - '0';
    else if ((*cp >= 'a' && *cp <= 'f'))
      c_val = *cp - 'a' + 10;
    else if ((*cp >= 'A' && *cp <= 'F'))
      c_val = *cp - 'A' + 10;
    else break;

    if (c_val > base) break;

    result = result * base + c_val;
    cp++;
  }

  if (endp)
    *endp = (char*)cp;

  return result;
}
