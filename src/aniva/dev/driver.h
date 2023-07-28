#ifndef __ANIVA_DRIVER__
#define __ANIVA_DRIVER__
#include <libk/stddef.h>
#include "core.h"
#include "libk/flow/reference.h"
#include "proc/socket.h"

struct vnode;
struct dev_manifest;

/*
 * Every type of driver has a version
 */
typedef union driver_version {
  struct {
    uint8_t maj;
    uint8_t min;
    uint16_t bump;
  };
  uint32_t version;
} driver_version_t;

#define DEF_DRV_VERSION(maj, min, bump) {{ (maj), (min), (bump) }}

typedef struct aniva_driver {
  const char m_name[MAX_DRIVER_NAME_LENGTH];
  const char m_descriptor[MAX_DRIVER_DESCRIPTOR_LENGTH];

  driver_version_t m_version;

  // TODO: make an actual framework for this lmao
  // FIXME: what arguments are best to pass here?
  SocketOnPacket f_drv_msg;

  int (*f_init)(void);
  int (*f_exit)(void);

  /* Used to try and verify if this driver supports a device */
  int (*f_probe)(struct aniva_driver* driver, void* device_info);

  DEV_TYPE m_type;
  uint32_t m_port;

  size_t m_dep_count;
  dev_url_t m_dependencies[];
} aniva_driver_t;

#define DRV_FS                      (0x00000001) /* Should be mounted inside the vfs */
#define DRV_NON_ESSENTIAL           (0x00000002) /* Is this a system-driver or user/nonessential driver */
#define DRV_ACTIVE                  (0x00000004) /* Is this driver available for opperations */
#define DRV_SOCK                    (0x00000008) /* Does this driver require a socket */
#define DRV_ALLOW_DYNAMIC_LOADING   (0x00000010) /* Allows the installed driver to be loaded when we need it */
#define DRV_HAS_HANDLE              (0x00000020) /* This driver is tethered to a handle */
#define DRV_FAILED                  (0x00000040) /* Failiure to load =/ */
#define DRV_DEFERRED                (0x00000080) /* This driver should be loaded at a later stage */
#define DRV_LIMIT_REACHED           (0x00000100) /* This driver could not be verified, since the limit of its tipe has been reached */

#define DRV_STAT_OK         (0)
#define DRV_STAT_INVAL      (-1)
#define DRV_STAT_NOMAN      (-2)
#define DRV_STAT_BUSY       (-3)

/*
aniva_driver_t* create_driver(
  const char* name,
  const char* descriptor,
  driver_version_t version,
  ANIVA_DRIVER_INIT init,
  ANIVA_DRIVER_EXIT exit,
  SocketOnPacket drv_msg,
  DEV_TYPE type
  );


void destroy_driver(aniva_driver_t* driver);
*/

#define driver_is_deferred(manifest) ((manifest->m_flags & DRV_DEFERRED) == DRV_DEFERRED)

struct vnode* create_fs_driver(struct dev_manifest* manifest);

void detach_fs_driver(struct vnode* node);

bool driver_is_ready(struct dev_manifest* manifest);

bool driver_is_busy(struct dev_manifest* manifest);

int drv_read(struct dev_manifest* manifest, void* buffer, size_t* buffer_size, uintptr_t offset);
int drv_write(struct dev_manifest* manifest, void* buffer, size_t* buffer_size, uintptr_t offset);

/*
 * Create a thread for this driver in the kernel process
 * and run the entry
 */
ErrorOrPtr bootstrap_driver(struct dev_manifest* manifest);

#endif //__ANIVA_DRIVER__
