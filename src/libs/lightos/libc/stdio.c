#include "stdio.h"
#include "errno.h"
#include "lightos/handle.h"
#include "lightos/api/sysvar.h"
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
    uint8_t* file_buffer;

    stdin = &__std_files[0];
    stdout = &__std_files[1];
    stderr = &__std_files[2];

    /* Allocate memory for the filebuffers */
    file_buffer = malloc(3 * buffer_size);

    if (!file_buffer)
        return -ENOMEM;

    /* Make sure no junk */
    memset(stdin, 0, sizeof(*stdin));
    memset(stdout, 0, sizeof(*stdout));
    memset(stderr, 0, sizeof(*stderr));

    /* Set stdin */
    stdin->handle = HNDL_INVAL;
    stdin->r_buf_size = buffer_size;
    /* Create buffer */
    stdin->r_buff = file_buffer + (0 * buffer_size);
    /* Make sure the buffer is empty empty */
    memset(stdin->r_buff, 0, buffer_size);

    /* Set stdout */
    stdout->handle = HNDL_INVAL;
    stdout->w_buf_size = buffer_size;
    /* Create buffer */
    stdout->w_buff = file_buffer + (1 * buffer_size);
    /* Make sure the buffer is empty empty */
    memset(stdout->w_buff, 0, buffer_size);

    /* Set stderr */
    stderr->handle = HNDL_INVAL;
    stderr->w_buf_size = buffer_size;
    /* Create buffer */
    stderr->w_buff = file_buffer + (2 * buffer_size);
    /* Make sure the buffer is empty empty */
    memset(stderr->w_buff, 0, buffer_size);

    return 0;
}

void __init_stdio(void)
{
    int error;
    HANDLE stdio_handle;
    HANDLE_TYPE type = HNDL_TYPE_NONE;
    char stdio_path[512] = { 0 };

    /* Prepare the stdio I/O buffers */
    if ((error = __init_stdio_buffers(FILE_BUFSIZE)))
        exit(error);

    /* Open the stdio path variable */
    stdio_handle = open_sysvar(SYSVAR_STDIO, HNDL_FLAG_R);

    if (handle_verify(stdio_handle))
        return;

    /* Read it's value */
    (void)sysvar_read(stdio_handle, stdio_path, sizeof(stdio_path));

    /* Close the handle */
    close_handle(stdio_handle);

    /* Open the handle type variable */
    stdio_handle = open_sysvar(SYSVAR_STDIO_HANDLE_TYPE, HNDL_FLAG_R);

    if (handle_verify(stdio_handle))
        return;

    /* Read which type of handle we need to open */
    (void)sysvar_read(stdio_handle, &type, sizeof(type));

    /* Close the handle */
    close_handle(stdio_handle);

    if (type == HNDL_TYPE_NONE)
        return;

    stdout->handle = open_handle(stdio_path, type, HNDL_FLAG_W, NULL);
    stdin->handle = open_handle(stdio_path, type, HNDL_FLAG_R, NULL);
    stderr->handle = open_handle(stdio_path, type, HNDL_FLAG_W, NULL);
}

/*
 * Write a single byte into the buffer of a stream
 */
int __write_byte(FILE* stream, uint64_t* counter, char byte)
{
    if (!stream->w_buff)
        return -1;

    stream->w_buff[stream->w_buf_written++] = byte;

    /* Sync the buffer if the max. buffersize is reached, or the byte is a null-char */
    if (stream->w_buf_written >= stream->w_buf_size || !byte)
        fflush(stream);

    if (counter)
        (*counter)++;

    return 0;
}

/*
 * Write a string of bytes into the buffer of a stream
 *
 * Probably unsafe, since it just continues until it finds a null-byte
 */
int __write_bytes(FILE* stream, uint64_t* counter, char* bytes)
{
    int result = 0;

    for (char* c = bytes;; c++) {
        result = __write_byte(stream, counter, *c);

        if (result < 0 || !(*c))
            break;
    }

    return result;
}

int __read_byte(FILE* stream, uint8_t* buffer)
{
    size_t r_size;

    if (!buffer || !stream->r_buff)
        return NULL;

    /* We have run out of local buffer space. Lets ask for some new */
    if (!stream->r_capacity) {

        memset(stream->r_buff, 0, stream->r_buf_size);

        /* Yikes */
        if (handle_read_ex(stream->handle, stream->r_buff, stream->r_buf_size, &r_size))
            return NULL;

        /* Yikes again */
        if (!r_size)
            return NULL;

        /* Reset the read offset */
        stream->r_offset = NULL;

        /* Set capacity for the next read */
        stream->r_capacity = r_size;
    }

    *buffer = stream->r_buff[stream->r_offset];

    stream->r_offset++;
    stream->r_capacity--;

    return 1;
}

int __read_bytes(FILE* stream, uint8_t* buffer, size_t size)
{
    int ret = 0;
    int result;

    for (uint64_t i = 0; i < size; i++) {
        result = __read_byte(stream, &buffer[i]);

        if (!result)
            break;

        ret++;
    }

    /* Return the amount of bytes read */
    return ret;
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
            _flags |= HNDL_FLAG_READACCESS;
            break;
        case 'w':
        case 'W':
            _flags |= HNDL_FLAG_WRITEACCESS;
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
    HANDLE hndl;
    uint32_t flags;
    uint32_t mode;

    if (parse_modes(modes, &flags, &mode))
        return nullptr;

    /* TODO: implement open flags and modes */
    hndl = open_handle(path, HNDL_TYPE_FILE, flags, mode);

    if (handle_verify(hndl))
        return nullptr;

    ret = malloc(sizeof(*ret));

    if (!ret) {
        close_handle(hndl);
        return nullptr;
    }

    memset(ret, 0, sizeof(*ret));

    ret->handle = hndl;
    ret->w_buff = malloc(FILE_BUFSIZE);
    ret->w_buf_size = FILE_BUFSIZE;
    ret->r_buff = malloc(FILE_BUFSIZE);
    ret->r_buf_size = FILE_BUFSIZE;

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

    fflush(file);

    if (close_handle(file->handle))
        return -EBADHANDLE;

    if (file->w_buff)
        free(file->w_buff);
    if (file->r_buff)
        free(file->r_buff);

    free(file);

    return 0;
}

/*
 * Flush the local write buffer for this
 * stream (file) to the kernel
 */
int fflush(FILE* file)
{

    if (!file || !file->w_buff)
        return -1;

    /*
     * The syscall should handle empty buffers or invalid handles
     * but we'll check here just to be safe
     */
    if (!file->w_buf_written || file->handle == HNDL_INVAL)
        return 0;

    /* Call the kernel to empty our buffer */
    (void)sys_write(file->handle, file->w_buff, file->w_buf_written);

    /* Reset the buffer */
    memset(file->w_buff, 0, file->w_buf_written);

    /* Reset the written count */
    file->w_buf_written = NULL;

    return 0;
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
    int r_count;
    uint8_t* i_buffer;

    if (!buffer || !size || !count || !file)
        return NULL;

    i_buffer = (uint8_t*)buffer;

    for (uint64_t i = 0; i < count; i++) {
        r_count = __read_bytes(file, i_buffer, size);

        i_buffer += r_count;

        /* Check this read */
        if (r_count != size)
            return i;
    }

    return count;
}

static size_t __file_get_offset(FILE* file, long offset, int whence)
{
    if (!file)
        return -1;

    if (file->w_buf_written)
        fflush(file);

    /* Make sure we've reset our local I/O trackers */
    file->r_offset = 0;
    file->r_capacity = 0;

    /* Do the syscall */
    return sys_seek(file->handle, offset, whence);
}

/*
 * Tell the kernel we want to read and write from a certain offset
 * within a file
 */
int fseek(FILE* file, long offset, int whence)
{
    uint64_t error;

    error = __file_get_offset(file, offset, whence);

    if (error)
        return error;

    return 0;
}

/*
 * Return the current offset within a file
 */
long ftell(FILE* file)
{
    return __file_get_offset(file, 0, SEEK_CUR);
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
        .w_buff = (uint8_t*)buf,
        .w_buf_size = size,
        .w_buf_written = 0,
        .handle = HNDL_INVAL,
    };

    return real_va_sprintf(0, &out, fmt, args);
}

int snprintf(char* buf, size_t size, const char* fmt, ...)
{
    int result;
    va_list args;
    va_start(args, fmt);

    FILE out = {
        .w_buff = (uint8_t*)buf,
        .w_buf_size = size,
        .w_buf_written = 0,
        .handle = HNDL_INVAL,
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

    if (!__read_byte(stream, &buffer))
        return -1;

    return buffer;
}

int fputc(int c, FILE* stream)
{
    return __write_byte(stream, NULL, c);
}

int puts(const char* str)
{
    __write_bytes(stdout, nullptr, (char*)str);
    __write_byte(stdout, nullptr, '\n');
    return 0;
}

int putchar(int c)
{
    return fputc(c, stdout);
}
