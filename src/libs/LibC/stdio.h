#ifndef __LIGHTENV_LIBC_STDIO__
#define __LIGHTENV_LIBC_STDIO__

#include "LibSys/handle_def.h"
#include <sys/types.h>

#define SEEK_SET 0
#define _STDIO_H

/*
 * Weird struct ...
 */
typedef struct _FILE { 
  HANDLE_t handle;

  uint8_t* w_buff; 
  size_t w_buf_size;
  size_t w_buf_written;

  uint8_t* r_buff; 
  size_t r_buf_size;
} FILE;

#ifdef __cplusplus
extern "C" {
#endif

extern FILE* stderr;
extern FILE* stdout;

#define stderr stderr

extern int fclose(FILE*);
extern int fflush(FILE*);
extern FILE* fopen(const char*, const char*);
extern int fprintf(FILE*, const char*, ...);
extern unsigned long long fread(void*, unsigned long long, unsigned long long, FILE*);
extern int fseek(FILE*, long, int);
extern long ftell(FILE*);
extern unsigned long long fwrite(const void*, unsigned long long, unsigned long long, FILE*);
extern void setbuf(FILE*, char*);

/*
 * Write a formatted string to the stream
 */
extern int fprintf(FILE *__restrict stream,
    const char* format, ...);

/*
 * Write a formatted string to stdout 
 */
extern int printf(const char* format, ...);
extern int sprintf(const char* s,
    const char* format, ...);
extern int vfprintf(FILE* stream, const char* fmt, char*);

extern int scanf (const char *__restrict __format, ...);
extern int sscanf (const char*__restrict __s,
    const char *__restrict __format, ...);

extern int printf(const char* fmt, ...);

extern int fputc(int c, FILE* stream);
extern int putchar (int c);

#ifdef __cplusplus
}
#endif

#endif // !__LIGHTENV_LIBC_STDIO__
