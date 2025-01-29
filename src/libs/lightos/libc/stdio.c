#include "stdio.h"
#include "lightos/api/sysvar.h"
#include "lightos/fs/file.h"
#include "lightos/handle.h"
#include "lightos/object.h"
#include "lightos/sysvar/var.h"
#include "stdarg.h"
#include "sys/types.h"
#include <lightos/api/handle.h>
#include <lightos/syscall.h>
#include <lightos/system.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define FILE_BUFSIZE (0x2000)

FILE __std_files[3] = { NULL };

FILE* stdin;
FILE* stdout;
FILE* stderr;

extern int real_va_sprintf(uint8_t, FILE*, const char*, va_list);
extern int real_va_scanf(FILE*, const char*, va_list);
extern int real_va_sscanf(const char* buffer, const char* fmt, va_list args);

static int __init_stdio_buffers(unsigned int buffer_size)
{
    stdin = &__std_files[0];
    stdout = &__std_files[1];
    stderr = &__std_files[2];

    /* Make sure no junk */
    memset(stdin, 0, sizeof(*stdin));
    memset(stdout, 0, sizeof(*stdout));
    memset(stderr, 0, sizeof(*stderr));

    /* Set the read chanel */
    FileSetBuffers(stdin, buffer_size, NULL);

    /* Set the write chanels */
    FileSetBuffers(stdout, NULL, buffer_size);
    FileSetBuffers(stderr, NULL, buffer_size);

    return 0;
}

void __init_stdio(void)
{
    int error;
    HANDLE stdio_handle;
    char stdio_path[512] = { 0 };

    /* Prepare the stdio I/O buffers */
    if ((error = __init_stdio_buffers(FILE_BUFSIZE)))
        exit(error);

    /*
     * Open the stdio path variable
     * TODO: Also change stdio to just a link object just like woh
     */
    stdio_handle = open_sysvar(SYSVAR_STDIO, HF_R);

    if (handle_verify(stdio_handle))
        return;

    /* Read it's value */
    (void)sysvar_read(stdio_handle, stdio_path, sizeof(stdio_path));

    /* Close the handle */
    close_handle(stdio_handle);

    /* Any oss object can be used as stdio */
    stdout->object = OpenObject(stdio_path, HF_W, OT_ANY, NULL);
    stdin->object = OpenObject(stdio_path, HF_R, OT_ANY, NULL);
    stderr->object = OpenObject(stdio_path, HF_RW, OT_ANY, NULL);
}

static int parse_modes(const char* modes, uint32_t* flags, uint32_t* mode)
{
    uint32_t _flags = NULL;
    enum HNDL_MODE _mode = HNDL_MODE_NORMAL;

    for (uint32_t i = 0; i < strlen(modes); i++) {
        char c = modes[i];

        switch (c) {
        case 'r':
        case 'R':
            _flags |= HF_READACCESS;
            break;
        case 'w':
        case 'W':
            _flags |= HF_WRITEACCESS;
            break;
        case 'c':
        case 'C':
            _mode = HNDL_MODE_CREATE;
            break;
        }
    }

    *flags = _flags;
    *mode = _mode;

    return 0;
}

/*
 * Open a file at the specified path with the specified modes
 * returns a FILE object that holds a local read- and write buffer
 */
FILE* fopen(const char* path, const char* modes)
{
    FILE* ret;
    uint32_t flags;
    uint32_t mode;

    if (parse_modes(modes, &flags, &mode))
        return nullptr;

    ret = OpenFile(path, flags, mode);

    /* Couldn't open this file */
    if (!FileIsValid(ret))
        return nullptr;

    return ret;
}

/*
 * Send the fclose syscall in order to close the
 * file stream and resources that where attached to it
 */
int fclose(FILE* file)
{
    if (!file)
        return -1;

    /* Flush the files write buffer */
    fflush(file);

    /* Close the file */
    CloseFile(file);

    /* Free the file memory */
    free(file);

    return 0;
}

/*
 * Flush the local write buffer for this
 * stream (file) to the kernel
 */
int fflush(FILE* file)
{
    return FileFlush(file);
}

/*
 * This takes bytes from stdin until we get a newline/EOF
 */
int scanf(const char* __restrict __format, ...)
{
    int result;
    va_list args;
    va_start(args, __format);

    result = real_va_scanf(stdout, __format, args);

    va_end(args);

    return result;
}

int sscanf(const char* buffer, const char* format, ...)
{
    int result;
    va_list args;
    va_start(args, format);

    result = real_va_sscanf(buffer, format, args);

    va_end(args);
    return result;
}

/*
 * Write a formated stream of bytes into a file
 */
int fprintf(FILE* stream, const char* str, ...)
{
    (void)stream;
    (void)str;

    exit_noimpl("fprintf");
    return NULL;
}

/*
 * Read a certain amount of bytes from a file into a buffer
 */
unsigned long long fread(void* buffer, unsigned long long size, unsigned long long count, FILE* file)
{
    size_t read_len = FileWriteEx(file, file->rd_bhead, buffer, size, count);
    file->rd_bhead += read_len;

    return read_len;
}

/*
 * Tell the kernel we want to read and write from a certain offset
 * within a file
 */
int fseek(FILE* file, long offset, int whence)
{
    u64 __offset = offset;

    return FileSeek(file, &__offset, whence);
}

/*
 * Return the current offset within a file
 */
long ftell(FILE* file)
{
    u64 result = 0;

    (void)FileSeek(file, &result, SEEK_CUR);

    return result;
}

/*
 * Write a certain amount of bytes from a buffer into a file
 */
unsigned long long fwrite(const void* buffer, unsigned long long size, unsigned long long count, FILE* file)
{
    (void)buffer;
    (void)size;
    (void)count;
    (void)file;
    exit_noimpl("fwrite");
    return NULL;
}

/*
 * Set the local buffer of a file to something else I guess?
 */
void setbuf(FILE* file, char* buf)
{
    (void)file;
    (void)buf;
    exit_noimpl("setbuf");
}

int clearerr(FILE* stream)
{
    (void)stream;
    exit_noimpl("clearerr");
    return 0;
}

int ferror(FILE* stream)
{
    (void)stream;
    exit_noimpl("ferror");
    return 0;
}

int feof(FILE* stream)
{
    (void)stream;
    exit_noimpl("feof");
    return 0;
}

int vfprintf(FILE* out, const char* str, char* f)
{
    exit_noimpl("vfprintf");
    return NULL;
}

int printf(const char* fmt, ...)
{
    int result;
    va_list args;
    va_start(args, fmt);

    result = real_va_sprintf(0, stdout, fmt, args);

    va_end(args);

    return result;
}

int vsnprintf(char* buf, size_t size, const char* fmt, va_list args)
{
    FILE out = {
        .wr_buff = (u8*)buf,
        .wr_bsize = size,
        0
    };

    return real_va_sprintf(0, &out, fmt, args);
}

int snprintf(char* buf, size_t size, const char* fmt, ...)
{
    int result;
    va_list args;
    va_start(args, fmt);

    FILE out = {
        .wr_buff = (u8*)buf,
        .wr_bsize = size,
        0
    };

    result = real_va_sprintf(0, &out, fmt, args);

    va_end(args);

    return result;
}

char* gets(char* buffer, size_t size)
{
    return fgets(buffer, size, stdin);
}

char* fgets(char* buffer, size_t size, FILE* stream)
{
    int c;
    char* ret;

    /* Cache the start of the string */
    ret = buffer;

    memset(buffer, 0, size);

    for (c = fgetc(stream); size; c = fgetc(stream)) {

        if (c < 0)
            break;

        if ((uint8_t)c == '\n')
            return ret;

        size--;
        *buffer++ = c;

        if (!c)
            return ret;
    }

    if (!size)
        return ret;

    return NULL;
}

int fgetc(FILE* stream)
{
    uint8_t buffer;

    if (!FileRead(stream, &buffer, 1))
        return -1;

    return buffer;
}

int fputc(int c, FILE* stream)
{
    if (!FileWrite(stream, &c, 1))
        return -EINVAL;

    return 0;
}

int puts(const char* str)
{
    size_t str_len = strlen(str);

    FileWrite(stdout, (u8*)str, str_len);
    FileWrite(stdout, "\n", 1);

    FileFlush(stdout);
    return 0;
}

int putchar(int c)
{
    return fputc(c, stdout);
}
