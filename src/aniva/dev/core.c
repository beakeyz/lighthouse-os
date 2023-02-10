#include "core.h"
#include "libk/linkedlist.h"
#include "driver.h"
#include "mem/kmalloc.h"
#include "libk/string.h"

#define DRIVER_TYPE_COUNT 5

typedef struct {
  DEV_TYPE_t m_type;
  list_t* m_drivers;
} driver_register_t;

static struct {
  driver_register_t m_driver_registries[DRIVER_TYPE_COUNT];
  size_t m_drivers_loaded;
} s_driver_register;

const static DEV_TYPE_t dev_types[] = {
  DT_DISK,
  DT_IO,
  DT_SOUND,
  DT_GRAPHICS,
  DT_OTHER,
  DT_DIAGNOSTICS,
  DT_SERVICE,
};

static driver_register_t *get_register_for_driver_type(DEV_TYPE_t type);
static driver_register_t create_driver_register(DEV_TYPE_t type);

void init_aniva_driver_register() {

  for (int i = 0; i < DRIVER_TYPE_COUNT; i++) {
    s_driver_register.m_driver_registries[i] = create_driver_register(dev_types[i]);
  }

  s_driver_register.m_drivers_loaded = 0;

  // TODO: load default drivers
}

void load_driver(aniva_driver_t* driver) {

  if (!validate_driver(driver)) {
    return;
  }

  driver_register_t *reg = get_register_for_driver_type(driver->m_type);

  list_append(reg->m_drivers, driver);

  // TODO: queue driver initialization somewhere
  driver->f_init();
}

void unload_driver(struct aniva_driver* driver) {
  if (!validate_driver(driver)) {
    return;
  }

  driver_register_t *reg = get_register_for_driver_type(driver->m_type);
  ErrorOrPtr result = list_indexof(reg->m_drivers, driver);

  if (result.m_status == ANIVA_SUCCESS) {
    // FIXME: this pagefaults right now, because of kfree in the list_remove function...
    list_remove(reg->m_drivers, (uint32_t)result.m_ptr);
  } else {
    // since it apparently is not in the list where it belongs,
    // we need to find it and get rid of it!
    // FIXME: this should be handled as an error
    for (int i = 0; i < DRIVER_TYPE_COUNT; i++) {
      driver_register_t *entry = &s_driver_register.m_driver_registries[i];
      if (entry->m_type == driver->m_type)
        continue;

      // let's reuse result here
      result = list_indexof(entry->m_drivers, driver);
      if (result.m_status == ANIVA_SUCCESS) {
        list_remove(entry->m_drivers, result.m_ptr);
        // so this would be a return statement
        break;
      }
    }
  }

  // ALSO MAKE SURE WE CALL THE EXIT FUNCTION
  driver->f_exit();
}

static driver_register_t create_driver_register(DEV_TYPE_t type) {
  driver_register_t ret = {
    .m_type = type,
    .m_drivers = init_list()
  };
  return ret;
}

static driver_register_t *get_register_for_driver_type(DEV_TYPE_t type) {
  for (int i = 0; i < DRIVER_TYPE_COUNT; i++) {
    driver_register_t* entry = &s_driver_register.m_driver_registries[i];
    if (entry->m_type == type) {
      return entry;
    }
  }
  return nullptr;
}