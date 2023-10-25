#include "framebuffer.h"

int generic_draw_rect(fb_info_t* info, uint32_t x, uint32_t y, uint32_t width, uint32_t height, fb_color_t clr)
{
  if (!info || !info->kernel_addr || !width || !height)
    return -1;

  const uint32_t bytes_per_pixel = info->bpp / 8;
  const uint32_t r_bits_offset = info->colors.red.offset_bits / 8;
  const uint32_t g_bits_offset = info->colors.green.offset_bits / 8;
  const uint32_t b_bits_offset = info->colors.blue.offset_bits / 8;
  const uint32_t a_bits_offset = info->colors.alpha.offset_bits / 8;

  uint32_t current_x_offset;
  uint32_t current_y_offset = y * info->pitch;

  for (uint32_t i = 0; i < height; i++) {

    current_x_offset = x;

    for (uint32_t j = 0; j < width; j++) {

      if (current_x_offset >= info->width)
        break;

      *(uint8_t volatile*)(info->kernel_addr + current_x_offset * bytes_per_pixel + current_y_offset + r_bits_offset) = clr.components.r;
      *(uint8_t volatile*)(info->kernel_addr + current_x_offset * bytes_per_pixel + current_y_offset + g_bits_offset) = clr.components.g;
      *(uint8_t volatile*)(info->kernel_addr + current_x_offset * bytes_per_pixel + current_y_offset + b_bits_offset) = clr.components.b;
      if (bytes_per_pixel > 3 || !a_bits_offset)
        *(uint8_t volatile*)(info->kernel_addr + current_x_offset * bytes_per_pixel + current_y_offset + a_bits_offset) = clr.components.a;

      current_x_offset++;
    }

    if (y + i >= info->height)
      break;

    current_y_offset += info->pitch;
  }

  return 0;
}
