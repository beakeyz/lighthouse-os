#ifndef __LIGHT_STORAGE_CONTROLLER__
#define __LIGHT_STORAGE_CONTROLLER__
#include "libk/linkedlist.h"

void init_storage_controller();

extern list_t* g_controllers;

#endif // !__LIGHT_STORAGE_CONTROLLER__