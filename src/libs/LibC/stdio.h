#ifndef __LIGHTENV_LIBC_STDIO__
#define __LIGHTENV_LIBC_STDIO__

#define SEEK_SET 0
#define _STDIO_H

typedef struct _FILE { 
  void* buff; 
} FILE;

#ifdef __cplusplus
extern "C" {
#endif

extern FILE* stderr;

#define stderr stderr

int fclose(FILE*);
int fflush(FILE*);
FILE* fopen(const char*, const char*);
int fprintf(FILE*, const char*, ...);
unsigned long long fread(void*, unsigned long long, unsigned long long, FILE*);
int fseek(FILE*, long, int);
long ftell(FILE*);
unsigned long long fwrite(const void*, unsigned long long, unsigned long long, FILE*);
void setbuf(FILE*, char*);
int vfprintf(FILE* stream, const char* fmt, char*);

int printf(const char* fmt, ...);

#ifdef __cplusplus
}
#endif

#endif // !__LIGHTENV_LIBC_STDIO__
