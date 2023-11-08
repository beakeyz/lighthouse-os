#ifndef __ANIVA_VDEV_CONNECTOR__
#define __ANIVA_VDEV_CONNECTOR__

#include <libk/stddef.h>

enum VDEV_CONNECTOR_TYPE {
  VGA = 0,
  DVI,
  DP,
  HDMI,
  USB,
};

/* Maximum amount of connectors on a single video device */
#define VCONNECTOR_MAX 8

#define VCON_FLAG_CONNECTED 0x00000001
#define VCON_FLAG_USED      0x00000002

typedef struct vdev_connector {
  enum VDEV_CONNECTOR_TYPE type;
  uint32_t flags;
} vdev_connector_t;

#endif // !__ANIVA_VDEV_CONNECTOR__
