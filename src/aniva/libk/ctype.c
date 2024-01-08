#include "ctype.h"

int isalnum(int c)
{
  return isalpha(c) || isdigit(c);
}

int isalpha(int c)
{
  return (isupper(c) || islower(c));
}

int isdigit(int c)
{
  return (c >= '0' && c <= '9');
}

int islower(int c)
{
  return (c >= 'a' && c <= 'z');
}

int isupper(int c)
{
  return (c >= 'A' && c <= 'Z');
}

int isprint(int c)
{
  return isgraph(c) || c == ' ';
}

int isgraph(int c)
{
  return (c >= '!' && c <= '~');
}

int iscntrl(int c)
{
  return ((c >= 0 && c <= 0x1f) || (c == 0x7f));
}

int ispunct(int c)
{
  return isgraph(c) && !isalnum(c);
}

int isspace(int c)
{
  return (c == '\f' || c == '\n' || c == '\r' || c == '\t' || c == '\v' || c == ' ');
}

int isxdigit(int c)
{
  return ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'));
}

int isascii(int c)
{
  return (c <= 0x7f);
}

int tolower(int c)
{
  if (islower(c))
    return c;

  return c + ('a' - 'A');
}

int toupper(int c)
{
  if (isupper(c))
    return c;

  return c - ('a' - 'A');
}

