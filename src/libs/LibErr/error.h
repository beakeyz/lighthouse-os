#ifndef __LIGHTENV_ERROR__
#define __LIGHTENV_ERROR__

#include <stdint.h>

/* TODO: */
typedef struct {

  union {
    int64_t status;
    uint64_t result;
  };
} Result;

#endif // !__LIGHTENV_ERROR__
