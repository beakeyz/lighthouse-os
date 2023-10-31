#include "dev/core.h"
#include "dev/debug/serial.h"
#include "dev/manifest.h"
#include "dev/video/device.h"
#include "dev/video/framebuffer.h"
#include "entry/entry.h"
#include "libk/flow/error.h"
#include "libk/multiboot.h"
#include "logging/log.h"
#include "mem/kmem_manager.h"
#include "libk/stddef.h"
#include "libk/string.h"
#include "proc/ipc/packet_response.h"
#include "sched/scheduler.h"
#include <dev/driver.h>

int fb_driver_init();
int fb_driver_exit();

/* Local framebuffer information for the driver */
fb_info_t info = { 0 };

fb_ops_t fb_ops = {
  .f_draw_rect = generic_draw_rect,
};

static int efi_fbmemmap(uintptr_t base, size_t* p_size) 
{
  size_t size;

  if (!base || !p_size)
    return -1;

  size = ALIGN_UP(info.size, SMALL_PAGE_SIZE);

  println("Mapping framebuffer");

  /* Map the framebuffer to the exact base described by the caller */
  Must(__kmem_alloc_ex(nullptr, nullptr, info.addr, base, size, KMEM_CUSTOMFLAG_NO_REMAP, KMEM_FLAG_KERNEL | KMEM_FLAG_WC | KMEM_FLAG_WRITABLE));

  *p_size = size;

  return 0;
}

static uint64_t fb_driver_msg(aniva_driver_t* driver, dcc_t code, void* buffer, size_t size, void* out_buffer, size_t out_size)
{
  switch (code) {
    case VIDDEV_DCC_BLT:
      {
        viddev_blt_t* blt;

        if (size != sizeof(*blt))
          return -1;

        blt = buffer;
        break;
      }
    case VIDDEV_DCC_MAPFB:
      {
        viddev_mapfb_t* mapfb;

        /* Quick size verify */
        if (size != sizeof(viddev_mapfb_t))
          return DRV_STAT_INVAL;

        mapfb = buffer;

        efi_fbmemmap(mapfb->virtual_base, mapfb->size);
        break;
      }
    case VIDDEV_DCC_GET_FBINFO:
      {
        if (!out_buffer || out_size != sizeof(fb_info_t))
          return -1;

        memcpy(out_buffer, &info, out_size);
        return DRV_STAT_OK;
      }
    default:
      return -1;
  }
  return DRV_STAT_OK;
}

static video_device_ops_t efi_devops = {
};

static video_device_t vdev = {
  .flags = VIDDEV_FLAG_FB | VIDDEV_FLAG_FIRMWARE,
  .max_connector_count = 0,
  .current_connector_count = 0,
  .ops = &efi_devops,
};

// FIXME: this driver only works for the multiboot fb that we get passed
// by the bootloaded. We need to create another driver for using an external 
// GPU
aniva_driver_t efifb_driver = {
  .m_name = "efifb",
  .m_precedence = DRV_PRECEDENCE_BASIC,
  .m_type = DT_GRAPHICS,
  .m_version = DRIVER_VERSION(0, 0, 1),
  .f_init = fb_driver_init,
  .f_exit = fb_driver_exit,
  .f_msg = fb_driver_msg,
};
EXPORT_DRIVER_PTR(efifb_driver);

/*
 * Generic EFI GOP framebuffer driver lmao
 * 
 * I do want this to be compliant with our video interface, so we need to register a 
 * video device with low precedence as a framebuffer device. The step we take:
 *
 * - Register the device
 * - Find the framebuffer
 * - Fill the fb_info struct
 * - Make sure the ops are in order
 * - Make sure we are exported so we can be used
 *
 */
int fb_driver_init() {

  struct multiboot_tag_framebuffer* fb;

  /* Check the early system logs */
  if (!(g_system_info.sys_flags & SYSFLAGS_HAS_FRAMEBUFFER))
    return -1;

  println("Initialized fb driver!");

  fb = g_system_info.firmware_fb;

  if (!fb)
    return -1;

  memset(&info, 0, sizeof(info));

  info.addr = fb->common.framebuffer_addr;
  info.pitch = fb->common.framebuffer_pitch;
  info.bpp = fb->common.framebuffer_bpp;
  info.width = fb->common.framebuffer_width;
  info.height = fb->common.framebuffer_height;
  info.size = info.pitch * info.height;
  info.kernel_addr = Must(__kmem_kernel_alloc(info.addr, info.size, NULL, KMEM_FLAG_WRITABLE | KMEM_FLAG_NOCACHE));

  info.colors.red.length_bits = fb->framebuffer_red_mask_size;
  info.colors.green.length_bits = fb->framebuffer_green_mask_size;
  info.colors.red.length_bits = fb->framebuffer_blue_mask_size;

  info.colors.red.offset_bits = fb->framebuffer_red_field_position;
  info.colors.green.offset_bits = fb->framebuffer_green_field_position;
  info.colors.blue.offset_bits = fb->framebuffer_blue_field_position;

  info.ops = &fb_ops;

  size_t fb_page_count = GET_PAGECOUNT(info.size);
  uintptr_t fb_start_idx = kmem_get_page_idx(info.addr);

  /* Mark the physical range used */
  kmem_set_phys_range_used(fb_start_idx, fb_page_count);

  /* Register the video device (install it on our manifest) */
  register_video_device(&efifb_driver, &vdev);

  try_activate_video_device(&vdev);
  return 0;
}

int fb_driver_exit() {

  //kmem_set_phys_range_free(fb_start_idx, fb_page_count);

  /* Remove the video device */
  unregister_video_device(&vdev);

  return 0;
}
