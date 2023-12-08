
#include "strings.h"
#include "stdlib.h"
#include <ctype.h>

int strcasecmp (const char* s1, const char* s2)
{
  for (; tolower(*s1) == tolower(*s2) && *s1; s1++, s2++);
  return *(unsigned char *)s1 - *(unsigned char *)s2;
}

int strncasecmp (const char* s1, const char* s2, size_t n)
{
  (void)s1;
  (void)s2;
  (void)n;
  exit_noimpl("strncasecmp");
  return -1;
}

