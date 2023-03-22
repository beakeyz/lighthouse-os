#ifndef __LIGNTENV_LIBC_STRING__
#define __LIGNTENV_LIBC_STRING__

#ifdef __cplusplus
extern "C" {
#endif

void* memcpy(void*, const void*, unsigned long long);
void* memset(void*, int, unsigned long long);
char* strcpy(char*, const char*);
unsigned long long strlen(const char*);

#ifdef __cplusplus
}
#endif

#endif // !__LIGNTENV_LIBC_STRING__
