#include "framebuffer.h"
#include "dev/video/device.h"
#include "mem/heap.h"

fb_info_t* create_fb_info(video_device_t* device, size_t priv_data_size)
{
  fb_info_t* info;

  info = kmalloc(sizeof(fb_info_t) + priv_data_size);

  info->parent = device;

  device->fbinfo = info;

  return info;
}

/* Clear the framebuffer memory, deallocate other shit */
void destroy_fb_info(fb_info_t* info)
{
  if (info->ops && info->ops->f_destroy)
    info->ops->f_destroy(info);

  kfree(info);
}
