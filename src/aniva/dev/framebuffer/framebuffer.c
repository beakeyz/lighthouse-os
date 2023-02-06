#include "framebuffer.h"
#include "dev/debug/serial.h"
#include "interupts/control/pic.h"
#include "mem/kmem_manager.h"
#include "libk/stddef.h"
#include "libk/string.h"

// hihi the entire kernel has access to this :clown:
static fb_data_t framebuffer_data;

// TODO: double buffers
void init_fb(struct multiboot_tag_framebuffer *mb_fb) {
  framebuffer_data.address = (void*)(uintptr_t)FRAMEBUFFER_VIRTUAL_BASE;
  framebuffer_data.phys_address = mb_fb->common.framebuffer_addr;
  framebuffer_data.height = mb_fb->common.framebuffer_height;
  framebuffer_data.width = mb_fb->common.framebuffer_width;
  framebuffer_data.bpp = mb_fb->common.framebuffer_bpp;
  framebuffer_data.pitch = mb_fb->common.framebuffer_pitch;
  framebuffer_data.memory_size = mb_fb->common.framebuffer_pitch * mb_fb->common.framebuffer_height;

  for (uintptr_t i = 0; i < framebuffer_data.memory_size; i+=SMALL_PAGE_SIZE) {
    kmem_map_page(nullptr, (uintptr_t)framebuffer_data.address + i, framebuffer_data.phys_address + i, KMEM_CUSTOMFLAG_GET_MAKE, KMEM_FLAG_WRITABLE);
  }

  // quick kinda color tester
  uint32_t color = 0xffffffff;
  for (size_t y = 0; y < get_global_framebuffer_data().height; ++y) {
    for (size_t x = 0; x < get_global_framebuffer_data().width; ++x) {
      draw_pixel(x, y, color);
    }
  }
}

void draw_pixel (uint64_t x, uint64_t y, uint32_t color) {
  *(uint32_t*)((uintptr_t)get_global_framebuffer_data().address + get_global_framebuffer_data().pitch * y + x * 4) = color;
}
    
fb_data_t get_global_framebuffer_data() {
    return framebuffer_data;
}
