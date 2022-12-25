#ifndef __LIGHT_DEVICE__ 
#define __LIGHT_DEVICE__
#include <libk/stddef.h>

struct device;

typedef uintptr_t (*DEVICE_WRITE) (
  struct device* this,
  void* buffer,
  size_t buffer_size
);

typedef uintptr_t (*DEVICE_READ) (
  struct device* this,
  void* buffer,
  size_t buffer_size
);

typedef struct device {
  // TODO:
  uint32_t m_major;
  uint32_t m_minor;

  DEVICE_READ fRead;
  DEVICE_WRITE fWrite;

} device_t;

// 

#endif // !
