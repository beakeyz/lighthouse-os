#ifndef __ANIVA_VIDEO_CORE__
#define __ANIVA_VIDEO_CORE__

#include <libk/stddef.h>

struct video_device;

/*
 * Initialize any caches for the video core to work
 */
void init_video();

void register_video_device(struct video_device* device);
void unregister_video_device(struct video_device* device);

struct video_device* get_active_video_device();

#endif // !__ANIVA_VIDEO_CORE__
