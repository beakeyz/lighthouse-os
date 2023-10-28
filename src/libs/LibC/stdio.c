#include "stdio.h"
#include "stdarg.h"
#include "sys/types.h"
#include <LibSys/system.h>
#include <LibSys/syscall.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define FILE_BUFSIZE    (0x2000)

FILE __stdin = {
  .handle = 0,
  .r_buf_size = FILE_BUFSIZE,
  .r_offset = 0,
  .r_buff = NULL,
  .r_capacity = NULL,
};

FILE __stdout = {
  .handle = 1,
  .w_buf_written = 0,
  .w_buf_size = FILE_BUFSIZE,
  //.r_buf_size = FILE_BUFSIZE,
};

/* Initialize this to POSIXefy our userspace =O */
FILE __stderr = {
  0
};

FILE* stdin;
FILE* stdout;
FILE* stderr;

extern int real_va_sprintf(uint8_t, FILE* , const char* , va_list);
extern int real_va_scanf(FILE*, const char*, va_list);
extern int real_va_sscanf(const char* buffer, const char* fmt, va_list args);

void __init_stdio(void)
{
  stdin = &__stdin;
  stdout = &__stdout;
  stderr = &__stderr;

  /* Create buffers */
  stdin->r_buff = malloc(FILE_BUFSIZE);
  stdout->w_buff = malloc(FILE_BUFSIZE);
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
  if (stream->w_buf_written >= stream->w_buf_size || byte == '\0') {
    fflush(stream);
  }

  if (counter)
    (*counter)++;

  return 0;
}

/*
 * Write a string of bytes into the buffer of a stream
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

  if (!buffer)
    return NULL;

  /* We have run out of local buffer space. Lets ask for some new */
  if (!stream->r_capacity) {

    memset(stream->r_buff, 0, stream->r_buf_size);

    /* Call the kernel deliver a new section of the stream */
    r_size = syscall_3(SYSID_READ, stream->handle, (uint64_t)((void*)stream->r_buff), stream->r_buf_size);

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

    ret += result;
  }

  /* Return the amount of bytes read */
  return ret;
}

/*
 * Send the fclose syscall in order to close the 
 * file stream and resources that where attached to it 
 */
int fclose(FILE* file) {

  if (!file)
    return -1;

  fflush(file);
  
  syscall_1(SYSID_CLOSE, file->handle);

  return 0;
}

/*
 * Open a file at the specified path with the specified modes
 * returns a FILE object that holds a local read- and write buffer
 */
FILE* fopen(const char* path, const char* modes) {
  (void)path;
  (void)modes;
  return nullptr;
}

/*
 * Flush the local write buffer for this 
 * stream (file) to the kernel
 */
int fflush(FILE* file) {

  if (!file || !file->w_buff) return -1;

  if (!file->w_buf_written)
    return 0;

  /* Call the kernel to empty our buffer */
  syscall_3(SYSID_WRITE, file->handle, (uint64_t)((void*)file->w_buff), file->w_buf_written);

  /* Reset the buffer */
  memset(file->w_buff, 0, file->w_buf_written);

  /* Reset the written count */
  file->w_buf_written = NULL;

  return 0;
}

/*
 * This takes bytes from stdin until we get a newline/EOF
 */
int scanf (const char *__restrict __format, ...)
{
  int result;
  va_list args;
  va_start(args, __format);

  result = real_va_scanf(stdout, __format, args);

  va_end(args);

  return result;
}

int sscanf (const char* buffer, const char* format, ...)
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
int fprintf(FILE* stream, const char* str, ...) {
  (void)stream;
  (void)str;
  return NULL;
}

/*
 * Read a certain amount of bytes from a file into a buffer
 */
unsigned long long fread(void* buffer, unsigned long long size, unsigned long long count, FILE* file) {

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

/*
 * Tell the kernel we want to read and write from a certain offset
 * within a file
 */
int fseek(FILE* file, long offset, int whence) {
  (void)file;
  (void)offset;
  (void)whence;
  return NULL;
}

/*
 * ???
 */
long ftell(FILE* file) {
  (void)file;
  return NULL;
}

/*
 * Write a certain amount of bytes from a buffer into a file 
 */
unsigned long long fwrite(const void* buffer, unsigned long long size, unsigned long long count, FILE* file) {
  (void)buffer;
  (void)size;
  (void)count;
  (void)file;
  return NULL;
}

/*
 * Set the local buffer of a file to something else I guess?
 */
void setbuf(FILE* file, char* buf) {
  (void)file;
  (void)buf;
}

int vfprintf(FILE* out, const char* str, char* f) {
  return NULL;
}

int printf(const char* format, ...) 
{
  int result;
  va_list args;
  va_start(args, format);

  result = real_va_sprintf(0, stdout, format, args);

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

  for (c = fgetc(stream); c && size; c = fgetc(stream)) {

    if (c < 0)
      break;

    if (!c || (uint8_t)c == '\n')
      return ret;

    size--;
    *buffer++ = c;

  }

  if (!size)
    return ret;

  return NULL;
}

int fgetc(FILE* stream)
{
  uint8_t buffer;

  if (!fread(&buffer, 1, 1, stream))
    return -1;

  return buffer;
}

int fputc(int c, FILE* stream)
{
  return __write_byte(stream, NULL, c);
}

int putchar (int c) 
{
  return fputc(c, stdout);
}
