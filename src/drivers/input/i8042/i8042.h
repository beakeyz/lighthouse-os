#ifndef __ANIVA_DRV_I8042__
#define __ANIVA_DRV_I8042__

#include "libk/io.h"
#include <libk/stddef.h>

#define I8042_DATA_PORT 0x60
#define I8042_STATUS_PORT 0x64
#define I8042_CMD_PORT 0x64

static inline uint32_t i8042_read_data()
{
  return in8(I8042_DATA_PORT);
}

static inline uint32_t i8042_read_status()
{
  return in8(I8042_STATUS_PORT);
}

static inline void i8042_write_data(uint8_t byte)
{
  out8(I8042_DATA_PORT, byte);
}

static inline void i8042_write_cmd(uint8_t byte)
{
  out8(I8042_CMD_PORT, byte);
}

#endif // !__ANIVA_DRV_I8042__
