#ifndef __ANIVA_DRIVER__
#define __ANIVA_DRIVER__
#include <libk/stddef.h>
#include "core.h"
#include "libk/flow/reference.h"
#include "proc/socket.h"

struct vnode;
struct dev_manifest;

typedef int (*ANIVA_DRIVER_INIT) ();
typedef int (*ANIVA_DRIVER_EXIT) ();

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

  uint32_t m_flags;

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
  struct dev_manifest* m_manifest;

  size_t m_dep_count;
  dev_url_t m_dependencies[];
} aniva_driver_t;

#define DRV_FS                      (0x00000001) /* Should be mounted inside the vfs */
#define DRV_NON_ESSENTIAL           (0x00000002) /* Is this a system-driver or user/nonessential driver */
#define DRV_ACTIVE                  (0x00000004) /* Is this driver available for opperations */
#define DRV_SOCK                    (0x00000008) /* Does this driver require a socket */
#define DRV_ALLOW_DYNAMIC_LOADING   (0x00000010) /* Allows the installed driver to be loaded when we need it */
#define DRV_HAS_HANDLE              (0x00000020) /* This driver is tethered to a handle */
#define DRV_FAILED                  (0x00000040)
#define DRV_DEFERRED                (0x00000080)

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

static inline bool driver_is_deferred(aniva_driver_t* drv)
{
  return ((drv->m_flags & DRV_DEFERRED) == DRV_DEFERRED);
}

struct vnode* create_fs_driver(aniva_driver_t* driver);

void detach_fs_driver(struct vnode* node);

void destroy_driver(aniva_driver_t* driver);

/*
 * Check properties of the driver to validate its integrity
 * TODO: more secure checking
 */
bool validate_driver(aniva_driver_t* driver);

bool driver_is_ready(aniva_driver_t* driver);

/*
 * Create a thread for this driver in the kernel process
 * and run the entry
 */
ErrorOrPtr bootstrap_driver(aniva_driver_t* driver, dev_url_t path);

#endif //__ANIVA_DRIVER__
