#include "stdio.h"
#include "stdarg.h"
#include "sys/types.h"
#include <LibSys/system.h>
#include <LibSys/syscall.h>
#include <stdlib.h>
#include <string.h>

#define FILE_BUFSIZE    (0x2000)

FILE __stdin = {
  .handle = 0,
  .r_buf_size = FILE_BUFSIZE,
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

  /* Sync the buffer if the max. buffersize is reached, or the byte is a newline char */
  if (stream->w_buf_written >= stream->w_buf_size || byte == '\n') {
    fflush(stream);
  }

  stream->w_buff[stream->w_buf_written++] = byte;

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

  for (char* c = bytes; *c; c++) {
    result = __write_byte(stream, counter, *c);

    if (result < 0)
      break;
  }

  return result;
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
  (void)buffer;
  (void)size;
  (void)count;
  (void)file;
  return NULL;
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

int putchar (int c) 
{

  return 0;
}
