#ifndef __LIBC_ERRNO__
#define __LIBC_ERRNO__

#include <lightos/error.h>

#ifndef KERNEL
extern int errno;
#endif // !KERNEL

#endif // !__LIBC_ERRNO__
