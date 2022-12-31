#include "AhciController.h"
#include <mem/kmalloc.h>

// TODO:
AhciDevice_t* init_ahcidevice(DeviceIdentifier_t* identifier) {
  AhciDevice_t* ret = kmalloc(sizeof(AhciDevice_t));

  ret->m_identifier = identifier;

  return ret;
}
