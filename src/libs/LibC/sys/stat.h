#ifndef __LIGHTOS_LIBC_STAT__
#define __LIGHTOS_LIBC_STAT__

typedef int mode_t;

/*!
 * @brief: Syscall wrapper to create a directory
 */
extern int mkdir(const char *pathname, mode_t mode);

#endif // !__LIGHTOS_LIBC_STAT__
