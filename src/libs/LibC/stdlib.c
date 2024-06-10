
#include "stdlib.h"
#include "lightos/syscall.h"
#include <ctype.h>
#include <lightos/system.h>
#include <math.h>
#include <mem/memory.h>
#include <string.h>

/*
 * Allocate a block of memory on the heap
 */
void* malloc(size_t __size)
{
    if (!__size)
        return nullptr;

    return mem_alloc(__size);
}

/*
 * Allocate count blocks of memory of __size bytes
 */
void* calloc(size_t count, size_t __size)
{
    if (!count)
        return nullptr;

    return mem_alloc(count * __size);
}

/*
 * Reallocate the block at ptr of __size bytes
 */
void* realloc(void* ptr, size_t __size)
{
    if (!ptr)
        return nullptr;

    if (!__size) {
        mem_dealloc(ptr);
        return nullptr;
    }

    return mem_move_alloc(ptr, __size);
}

/*
 * Free a block of memory allocated by malloc, calloc or realloc
 */
void free(void* ptr)
{
    if (!ptr)
        return;

    mem_dealloc(ptr);
}

int atoi(const char* str)
{
    int ret;
    bool negative;

    /* Skip spaces */
    while (isspace(*str))
        str++;

    negative = false;
    ret = 0;

    if (*str == '-')
        negative = true;

    while (isdigit(*str)) {
        /* Next digit */
        ret *= 10;
        /* Subtract, in order to get negative equivelant */
        ret -= (*str - '0');
        str++;
    }

    if (negative)
        return ret;

    return -ret;
}

/* Cheecky */
long atol(const char* nptr)
{
    return atoi(nptr);
}

/* FIXME: is this legal? */
long long atoll(const char* nptr)
{
    return atol(nptr);
}

double strtod(const char* nptr, char** endptr)
{
    int sign = 1;
    double exponent;

    if (*nptr++ == '-')
        sign = -1;

    int64_t decimal = 0;

    /* Compute the decimal part */
    while (*nptr && *nptr != '.') {
        if (!isdigit(*nptr))
            break;

        decimal *= 10LL;
        decimal += (int64_t)(*nptr - '0');

        nptr++;
    }

    double sub = 0;
    double mul = 0.1;

    if (*nptr != '.')
        goto do_exponent;

    nptr++;

    while (*nptr) {
        if (!isdigit(*nptr))
            break;

        sub += mul * (*nptr - '0');
        mul *= 0.1;
        nptr++;
    }

do_exponent:
    exponent = (double)sign;

    if (*nptr != 'e' && *nptr != 'E')
        goto finish_and_exit;

    nptr++;

    int exponent_sign = 1;

    if (*nptr == '+') {
        nptr++;
    } else if (*nptr == '-') {
        exponent_sign = -1;
        nptr++;
    }

    int exponent_part = 0;

    while (*nptr) {
        if (!isdigit(*nptr))
            break;

        exponent_part *= 10;
        exponent_part += (*nptr - '0');
        nptr++;
    }

    exponent = pow(10.0, (double)(exponent_part * exponent_sign));

finish_and_exit:

    if (endptr)
        *endptr = (char*)nptr;

    return ((double)decimal + sub) * exponent;
}

double atof(const char* nptr)
{
    return strtod(nptr, nullptr);
}

/*
 * Classically, the system libc call will simply execve the shell interpreter with
 * a fork() syscall, but we are too poor for that atm, so we will simply ask the kernel
 * real nicely to do this shit for us
 */
int system(const char* cmd)
{
    size_t cmd_len;

    if (!cmd || !(*cmd))
        return -1;

    cmd_len = strlen(cmd);

    if (!cmd_len)
        return -1;

    return syscall_2(SYSID_SYSEXEC, (uintptr_t)cmd, cmd_len);
}

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
