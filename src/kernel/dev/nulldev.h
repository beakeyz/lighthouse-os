#ifndef __LIGHT_NULLDEV__ 
#define __LIGHT_NULLDEV__

#include "dev/device.h"

typedef struct {
  device_t* m_dev; 
} nulldev_t;

nulldev_t init_nulldev();

#endif // !
