#ifndef __LIGHTENV_LIBC_TIME__
#define __LIGHTENV_LIBC_TIME__

#include <sys/types.h>

struct timespec {
  time_t    tv_sec;
  long      tv_nsec;
};

/* unistd.c */
extern int nanosleep(const struct timespec *req, struct timespec * rem);

#endif // !__LIGHTENV_LIBC_TIME__
