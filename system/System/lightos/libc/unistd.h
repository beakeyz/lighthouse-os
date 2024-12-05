#ifndef __LIGHTENV_LIBC_UNISTD__
#define __LIGHTENV_LIBC_UNISTD__

#include "sys/types.h"

#ifdef __cplusplus
extern "C" {
#endif

extern int execv(const char*, char* const[]);
extern int execve(const char*, char* const[], char* const[]);
extern int execp(const char*, char* const[]);
extern pid_t fork(void);
extern pid_t getpid(void);

extern uint32_t sleep(uint32_t seconds);
extern uint32_t usleep(uint32_t useconds);

extern int pause(void);
extern char* getcwd(char* buffer, unsigned int len);
extern int chdir(const char* newdir);

#ifdef __cplusplus
}
#endif

#endif // !__LIGHTENV_LIBC_UNISTD__
