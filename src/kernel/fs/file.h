#ifndef __LIGHT_FIL_IMPL__
#define __LIGHT_FIL_IMPL__
#include "fdiscriptor.h"

struct file;

typedef int (*FILE_IOCTL) (
  f_discriptor_t* discr,
  unsigned req,
  void* arg,
  ...
);

typedef enum {
  DEVICE,
  PTY,
  TTY,
  BLOCK_DEV,
  SOCKET,
  DEFAULT,
} file_type_t;

typedef struct file {
  file_type_t m_type;
  FILE_IOCTL fIoctl;
} file_t;

// TODO:

#endif // !__LIGHT_FIL_IMPL__
