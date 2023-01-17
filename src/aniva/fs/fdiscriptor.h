#ifndef __ANIVA_FDISCRIPTOR__
#define __ANIVA_FDISCRIPTOR__
#include <libk/stddef.h>

struct file;
struct f_discriptor;

typedef struct f_discriptor {

  struct file* m_file;

  struct {
    uint32_t _flags;
    bool _readable : 1;
    bool _writable : 1;
  } m_data;

} f_discriptor_t;

// TODO:

#endif // !__ANIVA_FDISCRIPTOR__
