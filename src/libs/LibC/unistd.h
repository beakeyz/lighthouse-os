#ifndef __LIGHTENV_LIBC_UNISTD__
#define __LIGHTENV_LIBC_UNISTD__

#include "sys/types.h"

#ifdef __cplusplus
extern "C" {
#endif

int execv(const char*, char* const[]);
int execve(const char*, char* const[], char* const[]);
int execp(const char*, char* const[]);
pid_t fork(void);
pid_t getpid(void);

#ifdef __cplusplus
}
#endif

#endif // !__LIGHTENV_LIBC_UNISTD__
