#include "nulldev.h"
#include "dev/device.h"
#include "mem/kmalloc.h"

static inline uintptr_t __read (device_t* this, void* buffer, size_t buffer_size);
static inline uintptr_t __write (device_t* this, void* buffer, size_t buffer_size);

nulldev_t init_nulldev() {
  nulldev_t dev = {0};

  device_t* _dev = kmalloc(sizeof(device_t));

  _dev->m_major = 1;
  _dev->m_minor = 3;

  _dev->fRead = __read;
  _dev->fWrite = __write;

  dev.m_dev = _dev;

  return dev;
}

static inline uintptr_t __read (device_t* this, void* buffer, size_t buffer_size) {
  return 0;
}

static inline uintptr_t __write (device_t* this, void* buffer, size_t buffer_size) {
  return buffer_size;
}
