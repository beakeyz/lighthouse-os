
#include "lightos/fs/file.h"
#include "stdarg.h"
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Print callback for when we want to get an actual byte onto the stream */
typedef int (*F_PRINT_CB)(char byte, char** out, size_t* p_cur_size, void* priv);

/*!
 * @brief: Print an unsigned number as a base-16 number
 */
static void _print_hex(uint64_t value, int prec, F_PRINT_CB output_cb, char** out, size_t* max_size, void* priv)
{
    uint32_t idx;
    char tmp[128];
    const char hex_chars[17] = "0123456789ABCDEF";
    uint8_t len = 1;

    memset(tmp, '0', sizeof(tmp));

    uint64_t size_test = value;
    while (size_test / 16 > 0) {
        size_test /= 16;
        len++;
    }

    if (len < prec && prec && prec != -1)
        len = prec;

    idx = 0;

    while (value / 16 > 0) {
        uint8_t remainder = value % 16;
        value -= remainder;
        tmp[len - 1 - idx] = hex_chars[remainder];
        idx++;
        value /= 16;
    }
    uint8_t last = value % 16;
    tmp[len - 1 - idx] = hex_chars[last];

    for (uint32_t i = 0; i < len; i++)
        output_cb(tmp[i], out, max_size, priv);
}

/*!
 * @brief: Print an unsigned number as a base-10 number
 */
static inline int _print_decimal(int64_t value, int prec, F_PRINT_CB output_cb, char** out, size_t* max_size, void* priv)
{
    char tmp[128];
    uint8_t size = 1;

    memset(tmp, '0', sizeof(tmp));

    /* Test the size of this value */
    uint64_t size_test = value;
    while (size_test / 10 > 0) {
        size_test /= 10;
        size++;
    }

    if (size < prec && prec && prec != -1)
        size = prec;

    uint8_t index = 0;

    while (value / 10 > 0) {
        uint8_t remain = value % 10;
        value /= 10;
        tmp[size - 1 - index] = remain + '0';
        index++;
    }
    uint8_t remain = value % 10;
    tmp[size - 1 - index] = remain + '0';

    for (uint32_t i = 0; i < size; i++)
        output_cb(tmp[i], out, max_size, priv);

    return size;
}

/*!
 * @brief: Scaffolding function for raw printing onto byte buffers/streams
 */
static inline int _vprintf(const char* fmt, va_list args, F_PRINT_CB output_cb, char** out, size_t max_size, void* priv)
{
    int prec;
    uint32_t integer_width;
    uint32_t max_length;

    for (const char* c = fmt; *c; c++) {

        if (*c != '%')
            goto kputch_cycle;

        integer_width = 0;
        max_length = 0;
        prec = -1;
        c++;

        if (*c == '-')
            c++;

        /* FIXME: Register digits at the start */
        while (isdigit(*c)) {
            max_length = (max_length * 10) + ((*c) - '0');
            c++;
        }

        if (*c == '.') {
            prec = 0;
            c++;

            while (*c >= '0' && *c <= '9') {
                prec *= 10;
                prec += *c - '0';
                c++;
            }
        }

        /* Account for size */
        while (*c == 'l' && integer_width < 2) {
            integer_width++;
            c++;
        }

        switch (*c) {
        case 'i':
        case 'd': {
            int64_t value;
            switch (integer_width) {
            case 0:
                value = va_arg(args, int);
                break;
            case 1:
                value = va_arg(args, long);
                break;
            case 2:
                value = va_arg(args, long long);
                break;
            default:
                value = 0;
            }

            if (value < 0) {
                value = -value;
                output_cb('-', out, &max_size, priv);
            }

            _print_decimal(value, prec, output_cb, out, &max_size, priv);
            break;
        }
        case 'p':
            integer_width = 2;
        case 'x':
        case 'X': {
            uint64_t value;
            switch (integer_width) {
            case 0:
                value = va_arg(args, int);
                break;
            case 1:
                value = va_arg(args, long);
                break;
            case 2:
                value = va_arg(args, long long);
                break;
            default:
                value = 0;
            }

            _print_hex(value, prec, output_cb, out, &max_size, priv);
            break;
        }
        case 's': {
            const char* str = va_arg(args, char*);

            if (!max_length)
                while (*str)
                    output_cb(*str++, out, &max_size, priv);
            else
                while (max_length--)
                    if (*str)
                        output_cb(*str++, out, &max_size, priv);
                    else
                        output_cb(' ', out, &max_size, priv);

            break;
        }
        default: {
            output_cb(*c, out, &max_size, priv);
            break;
        }
        }

        continue;

    kputch_cycle:
        output_cb(*c, out, &max_size, priv);
    }

    /* End with a null-byte */
    output_cb(NULL, out, &max_size, priv);

    return max_size;
}

static int __sprintf_cb(char c, char** out, size_t* p_cur_size, void* priv)
{
    FILE* stream = priv;

    if (FileWrite(stream, &c, sizeof(c)) == 0)
        return -1;
    return 0;
}

int real_va_sprintf(uint8_t mode, FILE* stream, const char* fmt, va_list va)
{
    return _vprintf(fmt, va, __sprintf_cb, NULL, 0, stream);
}
