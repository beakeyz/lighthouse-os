#ifndef __ANIVA_VIDEO_DEVICE_EVENTS__
#define __ANIVA_VIDEO_DEVICE_EVENTS__

struct video_device;

enum VDEV_EVENT_TYPE {
    VDEV_EVENT_REMOVE,
    VDEV_EVENT_REGISTER,
    VDEV_EVENT_VBLANK,
};

/*
 * The base context used in every vdev event
 */
typedef struct vdev_event_ctx {
    enum VDEV_EVENT_TYPE type;

    struct video_device* device;
} vdev_event_ctx_t;

#endif // !__ANIVA_VIDEO_DEVICE_EVENTS__
