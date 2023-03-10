#ifndef __ANIVA_DRIVER__
#define __ANIVA_DRIVER__
#include <libk/stddef.h>
#include "core.h"
#include "proc/socket.h"

typedef void (*ANIVA_DRIVER_INIT) ();
typedef int (*ANIVA_DRIVER_EXIT) ();
//typedef uintptr_t (*ANIVA_DRIVER_DRV_MSG) (void* buffer, size_t buffer_size);

typedef struct driver_version {
  uint8_t maj;
  uint8_t min;
  uint16_t bump;
} driver_version_t;

typedef struct driver_identifier {
  uint8_t m_minor;
  uint8_t m_major;
} driver_identifier_t;

typedef struct aniva_driver {
  const char m_name[MAX_DRIVER_NAME_LENGTH];
  const char m_descriptor[MAX_DRIVER_DESCRIPTOR_LENGTH];
  driver_version_t m_version;
  driver_identifier_t m_ident;

  ANIVA_DRIVER_INIT f_init;
  ANIVA_DRIVER_EXIT f_exit;

  // TODO: make an actual framework for this lmao
  // FIXME: what arguments are best to pass here?
  SocketOnPacket f_drv_msg;

  DEV_TYPE m_type;
  uint32_t m_port;

  // TODO: driver resources and dependencies
  // TODO: we can do dependency checking when drivers try to
  // send packets to other drivers
  size_t m_dep_count;
  dev_url_t m_dependencies[];
} aniva_driver_t;

aniva_driver_t* create_driver(
  const char* name,
  const char* descriptor,
  driver_version_t version,
  driver_identifier_t identifier,
  ANIVA_DRIVER_INIT init,
  ANIVA_DRIVER_EXIT exit,
  SocketOnPacket drv_msg,
  DEV_TYPE type
  );

void destroy_driver(aniva_driver_t* driver);

/*
 * Check properties of the driver to validate its integrity
 * TODO: more secure checking
 */
bool validate_driver(aniva_driver_t* driver);

/*
 * Create a thread for this driver in the kernel process
 * and run the entry
 */
ErrorOrPtr bootstrap_driver(aniva_driver_t* driver);

#endif //__ANIVA_DRIVER__
