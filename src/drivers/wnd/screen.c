#include "screen.h"
#include "drivers/wnd/window.h"
#include "libk/data/bitmap.h"
#include "mem/heap.h"
#include "mem/kmem_manager.h"


lwnd_screen_t* create_lwnd_screen(fb_info_t* fb, uint16_t max_window_count)
{
  lwnd_screen_t* ret;

  if (!fb)
    return nullptr;

  if (!max_window_count || max_window_count > LWND_SCREEN_MAX_WND_COUNT)
    max_window_count = LWND_SCREEN_MAX_WND_COUNT;

  /* We can simply allocate this fucker on the kernel heap, cuz we're cool like that */
  ret = kmalloc(sizeof(*ret) + (sizeof(lwnd_window_t*) * max_window_count));

  if (!ret)
    return nullptr;

  memset(ret, 0, sizeof(*ret));

  ret->info = fb;
  ret->width = fb->width;
  ret->height = fb->height;
  ret->max_window_count = max_window_count;

  /* This can get quite big once the resolution gets bigger */
  ret->pixel_bitmap = create_bitmap_ex(fb->width * fb->height, 0);
  ret->window_stack_size = ALIGN_UP(max_window_count * sizeof(lwnd_window_t*), SMALL_PAGE_SIZE);
  ret->window_stack = (lwnd_window_t**)Must(__kmem_kernel_alloc_range(ret->window_stack_size, NULL, KMEM_FLAG_KERNEL | KMEM_FLAG_WRITABLE));

  return ret;
}

void destroy_lwnd_screen(lwnd_screen_t* screen)
{
  if (!screen)
    return;

  destroy_bitmap(screen->pixel_bitmap);
  Must(__kmem_kernel_dealloc((uint64_t)screen->window_stack, screen->window_stack_size));
  kfree(screen);
}
