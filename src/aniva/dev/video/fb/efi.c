#include "dev/core.h"
#include "dev/endpoint.h"
#include "dev/manifest.h"
#include "dev/video/device.h"
#include "dev/video/framebuffer.h"
#include "entry/entry.h"
#include "libk/flow/error.h"
#include "libk/multiboot.h"
#include "mem/kmem_manager.h"
#include "libk/stddef.h"
#include <dev/driver.h>

/*
 * Driver for the EFI framebuffer protocols
 *
 * These include GOP and UGA, but we mostly focus on GOP.
 * The aim of this driver is simply to provide the most basic vdev interface in the system, 
 * for when there are no more sophisticated drivers available to support more sophisticated
 * features.
 *
 * Here, we (should) simply support the following things:
 *  - Managing the framebuffer memory
 *  - Getting information about the framebuffer
 *  - Simple blt opperations
 * 
 * Things we lack are:
 *  - 2D/3D acceleration, in the form of hardware blt, 3D engines, ect.
 *  - Advanced video memory management
 *  - HW cursor
 */

int fb_driver_init();
int fb_driver_exit();

/* Our manifest */
static drv_manifest_t* _this;
static video_device_t* _vdev;
/* Local framebuffer information for the driver */
static fb_info_t* _main_info = nullptr;


static int efifb_get_info(device_t* dev, vdev_info_t* info)
{
  /* TODO: */
  memset(info, 0, sizeof(*info));
  return 0;
}

/*!
 * @brief: Map the framebuffer to a virtual address
 *
 * TODO: keep track of all the mappings
 */
static ssize_t efifb_map(device_t* dev, fb_handle_t fb, uint32_t x, uint32_t y, uint32_t width, uint32_t height, vaddr_t base) 
{
  ssize_t size;
  fb_info_t* info;
  video_device_t* vdev;
  fb_helper_t* helper;

  /* base should be page aligned */
  if (!base || ALIGN_DOWN(base, SMALL_PAGE_SIZE) != base)
    return -1;

  vdev = dev->private;
  helper = vdev->fb_helper;

  if (!helper)
    return -2;

  /* Grab the right info */
  info = fb_helper_get(helper, fb);
  size = 0;

  if (!width && !height)
    size = ALIGN_UP(info->size, SMALL_PAGE_SIZE);

  if (size)
    /* Map the framebuffer to the exact base described by the caller */
    Must(__kmem_alloc_ex(nullptr, nullptr, info->addr + x * BYTES_PER_PIXEL(info->bpp) + y * info->pitch, base, size, KMEM_CUSTOMFLAG_NO_REMAP, KMEM_FLAG_WC | KMEM_FLAG_WRITABLE));
  else
    kernel_panic("TODO: handle size = 0 (efifb_map)");

  /* Make sure we can get the mapping */
  fb_helper_add_mapping(helper, x, y, width, height, size, base);

  return size;
}

static ssize_t efifb_unmap(device_t* dev, fb_handle_t fb, vaddr_t base) 
{
  ssize_t size;
  video_device_t* vdev;
  fb_helper_t* helper;
  fb_mapping_t* mapping;

  if (!base)
    return -1;

  vdev = dev->private;
  helper = vdev->fb_helper;
  mapping = NULL;

  if (!helper)
    return -2;

  if (fb_helper_get_mapping(helper, base, &mapping) || !mapping)
    return -3;

  /* Unmap the page range */
  kmem_unmap_range(nullptr, base, mapping->size);

  size = mapping->size;

  /* This really can't fail lmao */
  ASSERT(KERR_OK(fb_helper_remove_mapping(helper, base)));

  return size;
}

static uint64_t fb_driver_msg(aniva_driver_t* driver, dcc_t code, void* buffer, size_t size, void* out_buffer, size_t out_size)
{
  kernel_panic("TODO: (efi) fb_driver_msg");
  return DRV_STAT_OK;
}

int efifb_remove(video_device_t* device)
{
  size_t fb_page_count;
  uintptr_t fb_start_idx;

  /* Calculate things again */
  fb_page_count = GET_PAGECOUNT(_main_info->addr, _main_info->size);
  fb_start_idx = kmem_get_page_idx(_main_info->addr);

  /* TODO: tell underlying hardware to stop its EFI framebuffer engine */
  kernel_panic("TODO: actual efifb_remove");

  /* Mark the physical range used */
  kmem_set_phys_range_free(fb_start_idx, fb_page_count);
  return 0;
}

static int efifb_get_fb(device_t* dev, fb_handle_t* handle)
{
  video_device_t* vdev;

  if (!dev || !handle)
    return -1;

  vdev = dev->private;

  if (!vdev || !vdev->fb_helper)
    return -2;

  *handle = vdev->fb_helper->main_fb;
  return 0;
}

static int efifb_set_fb(device_t* dev, fb_handle_t fb)
{
  video_device_t* vdev;

  if (!dev)
    return -1;

  vdev = dev->private;

  if (!vdev || !vdev->fb_helper)
    return -2;

  if (fb >= vdev->fb_helper->fb_capacity)
    return -3;

  vdev->fb_helper->main_fb = fb;
  return 0;
}

fb_helper_ops_t _efifb_helper_ops = {
  .f_get_main_fb = efifb_get_fb,
  .f_set_main_fb = efifb_set_fb,
  .f_map = efifb_map,
  .f_unmap = efifb_unmap,
};

static struct device_video_endpoint _efi_vdev_ep = {
  .f_get_info = efifb_get_info,
};

static device_ep_t _efi_endpoints[] = {
  { ENDPOINT_TYPE_VIDEO, sizeof(_efi_vdev_ep), { &_efi_vdev_ep } },
  { NULL, },
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

static inline void _init_main_info(struct multiboot_tag_framebuffer* fb)
{
  /* Construct fbinfo ourselves */
  _main_info->addr = fb->common.framebuffer_addr;
  _main_info->pitch = fb->common.framebuffer_pitch;
  _main_info->bpp = fb->common.framebuffer_bpp;
  _main_info->width = fb->common.framebuffer_width;
  _main_info->height = fb->common.framebuffer_height;
  _main_info->size = _main_info->pitch * _main_info->height;
  _main_info->kernel_addr = Must(__kmem_kernel_alloc(_main_info->addr, _main_info->size, NULL, KMEM_FLAG_WRITABLE | KMEM_FLAG_NOCACHE));

  _main_info->colors.red.length_bits = fb->framebuffer_red_mask_size;
  _main_info->colors.green.length_bits = fb->framebuffer_green_mask_size;
  _main_info->colors.red.length_bits = fb->framebuffer_blue_mask_size;

  _main_info->colors.red.offset_bits = fb->framebuffer_red_field_position;
  _main_info->colors.green.offset_bits = fb->framebuffer_green_field_position;
  _main_info->colors.blue.offset_bits = fb->framebuffer_blue_field_position;
}

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
int fb_driver_init() 
{
  video_device_t* vdev;
  struct multiboot_tag_framebuffer* fb;

  _this = try_driver_get(&efifb_driver, NULL);

  ASSERT_MSG(_this, "Could not get the efifb driver manifest!");

  /* Check the early system logs. This is okay, but we really do want to get an actual gpu driver running soon lol */
  if (!(g_system_info.sys_flags & SYSFLAGS_HAS_FRAMEBUFFER))
    return 0;

  fb = g_system_info.firmware_fb;

  if (!fb)
    return -1;

  vdev = create_video_device(&efifb_driver, VIDDEV_MAINDEVICE, _efi_endpoints);

  if (!vdev)
    return -2;

  /* Initialize a framebuffer helper for a single framebuffer */
  vdev_init_fb_helper(vdev, 1, &_efifb_helper_ops);

  /* Grab the info */
  _main_info = fb_helper_get(vdev->fb_helper, 0);

  /* Initialize it's fields */
  _init_main_info(fb);

  ASSERT(video_deactivate_current_driver() == 0);

  /* Register the video device (install it on our manifest) */
  register_video_device(vdev);

  _vdev = vdev;
  return 0;
}

/*!
 * @brief: Destruct any efi shit
 *
 * TODO: make sure any users of this driver are notified of it's destruction
 */
int fb_driver_exit() 
{
  unregister_video_device(_vdev);

  destroy_video_device(_vdev);
  return 0;
}
