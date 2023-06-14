#include "stdio.h"
#include "sys/types.h"

/* Initialize this to POSIXefy our userspace =O */
FILE* stderr;

int fclose(FILE* file) {
  (void)file;
  return NULL;
}

int fflush(FILE* file) {
  (void)file;
  return NULL;
}

FILE* fopen(const char* path, const char* mode) {
  (void)path;
  (void)mode;
  return nullptr;
}

int fprintf(FILE* stream, const char* str, ...) {
  (void)stream;
  (void)str;
  return NULL;
}

unsigned long long fread(void* buffer, unsigned long long size, unsigned long long count, FILE* file) {
  (void)buffer;
  (void)size;
  (void)count;
  (void)file;
  return NULL;
}

int fseek(FILE* file, long offset, int whence) {
  (void)file;
  (void)offset;
  (void)whence;
  return NULL;
}

long ftell(FILE* file) {
  (void)file;
  return NULL;
}

unsigned long long fwrite(const void* buffer, unsigned long long size, unsigned long long count, FILE* file) {
  (void)buffer;
  (void)size;
  (void)count;
  (void)file;
  return NULL;
}

void setbuf(FILE* file, char* buf) {
  (void)file;
  (void)buf;
}

int vfprintf(FILE* out, const char* str, char* f) {
  return NULL;
}
