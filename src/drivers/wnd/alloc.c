
#include "alloc.h"
#include "drivers/wnd/screen.h"
#include "drivers/wnd/window.h"
#include "mem/zalloc.h"

static zone_allocator_t* _window_allocator;
static zone_allocator_t* _screen_allocator;

int init_lwnd_alloc()
{
  _window_allocator = create_zone_allocator(64 * Kib, sizeof(lwnd_window_t), NULL);

  if (!_screen_allocator)
    return -1;

  _screen_allocator = create_zone_allocator(64 * Kib, sizeof(lwnd_screen_t), NULL);

  if (!_screen_allocator)
    return -1;

  return 0;
}

lwnd_window_t* allocate_lwnd_window()
{
  return zalloc_fixed(_window_allocator);
}

void deallocate_lwnd_window(lwnd_window_t* window)
{
  zfree_fixed(_window_allocator, window);
}

lwnd_screen_t* allocate_lwnd_screen()
{
  return zalloc_fixed(_screen_allocator);
}

void deallocate_lwnd_screen(lwnd_screen_t* screen)
{
  zfree_fixed(_screen_allocator, screen);
}
