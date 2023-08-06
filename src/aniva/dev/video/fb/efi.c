#include "dev/core.h"
#include "dev/debug/serial.h"
#include "dev/manifest.h"
#include "dev/video/device.h"
#include "dev/video/framebuffer.h"
#include "entry/entry.h"
#include "interrupts/control/pic.h"
#include "libk/flow/error.h"
#include "libk/multiboot.h"
#include "mem/kmem_manager.h"
#include "libk/stddef.h"
#include "libk/string.h"
#include "proc/ipc/packet_response.h"
#include "sched/scheduler.h"
#include <dev/driver.h>

int fb_driver_init();
int fb_driver_exit();

static int __fbinfo_set_colors(fb_info_t* info, struct multiboot_tag_framebuffer* fb)
{
  /* TODO */
  println(to_string(fb->framebuffer_red_mask_size));
  println(to_string(fb->framebuffer_green_mask_size));
  println(to_string(fb->framebuffer_blue_mask_size));

  println(to_string(fb->framebuffer_red_field_position));
  println(to_string(fb->framebuffer_green_field_position));
  println(to_string(fb->framebuffer_blue_field_position));

  println(to_string(fb->framebuffer_palette_num_colors));

  return 0;
}

static int efi_fbmemmap(fb_info_t* info, void* p_buffer, size_t* p_size) 
{
  size_t size;
  uintptr_t virtual_base;

  if (!p_buffer || !p_size)
    return -1;

  size = ALIGN_UP(info->size, SMALL_PAGE_SIZE);
  virtual_base = (uintptr_t)p_buffer;

  println("Mapping framebuffer");

  Must(__kmem_alloc_ex(nullptr, nullptr, info->addr, virtual_base, size, KMEM_CUSTOMFLAG_NO_REMAP, NULL));

  *p_size = size;

  return 0;
}

static int efi_memmap(video_device_t* device, void* p_buffer, size_t* p_size) 
{
  return efi_fbmemmap(device->fbinfo, p_buffer, p_size);
}

static fb_ops_t efi_fbops = {
  .f_mmap = efi_fbmemmap,
};

static video_device_ops_t efi_devops = {
  .f_mmap = efi_memmap,
};

static video_device_t vdev = {
  .flags = VIDDEV_FLAG_FB | VIDDEV_FLAG_FIRMWARE,
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

  fb_info_t* info;
  struct multiboot_tag_framebuffer* fb;

  /* Check the early system logs */
  if (!g_system_info.has_framebuffer)
    return -1;

  println("Initialized fb driver!");

  fb = get_mb2_tag((void*)g_system_info.multiboot_addr, MULTIBOOT_TAG_TYPE_FRAMEBUFFER);

  if (!fb)
    return -1;

  info = create_fb_info(&vdev, NULL);

  if (!info)
    return -1;

  info->addr = fb->common.framebuffer_addr;
  info->pitch = fb->common.framebuffer_pitch;
  info->bpp = fb->common.framebuffer_bpp;
  info->width = fb->common.framebuffer_width;
  info->height = fb->common.framebuffer_height;
  info->size = info->pitch * info->height;

  info->ops = &efi_fbops;

  __fbinfo_set_colors(info, fb);

  size_t fb_page_count = GET_PAGECOUNT(info->size);
  uintptr_t fb_start_idx = kmem_get_page_idx(info->addr);

  /* Mark the physical range used */
  kmem_set_phys_range_used(fb_start_idx, fb_page_count);

  /* Register the video device (install it on our manifest) */
  register_video_device(&efifb_driver, &vdev);

  return 0;
}

int fb_driver_exit() {

  unregister_video_device(&vdev);

  return 0;
}
