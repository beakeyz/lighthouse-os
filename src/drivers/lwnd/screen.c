#include "screen.h"
#include "dev/video/framebuffer.h"
#include "drivers/lwnd/alloc.h"
#include "drivers/lwnd/event.h"
#include "drivers/lwnd/window.h"
#include "libk/data/bitmap.h"
#include "libk/flow/error.h"
#include "libk/string.h"
#include "logging/log.h"
#include "mem/heap.h"
#include "mem/kmem_manager.h"
#include "sync/mutex.h"

/*!
 * @brief: Allocate memory for a screen
 */
lwnd_screen_t* create_lwnd_screen(fb_info_t* fb, uint16_t max_window_count)
{
  lwnd_screen_t* ret;

  if (!fb)
    return nullptr;

  if (!max_window_count || max_window_count > LWND_SCREEN_MAX_WND_COUNT)
    max_window_count = LWND_SCREEN_MAX_WND_COUNT;

  /* We can simply allocate this fucker on the kernel heap, cuz we're cool like that */
  ret = allocate_lwnd_screen();

  if (!ret)
    return nullptr;

  memset(ret, 0, sizeof(*ret));

  ret->info = fb;
  ret->width = fb->width;
  ret->height = fb->height;
  ret->max_window_count = max_window_count;

  ret->event_lock = create_mutex(NULL);
  ret->draw_lock = create_mutex(NULL);

  /* This can get quite big once the resolution gets bigger */
  ret->pixel_bitmap = create_bitmap_ex(fb->width * fb->height, 0);
  ret->window_stack_size = ALIGN_UP(max_window_count * sizeof(lwnd_window_t*), SMALL_PAGE_SIZE);
  ret->window_stack = (lwnd_window_t**)Must(__kmem_kernel_alloc_range(ret->window_stack_size, NULL, KMEM_FLAG_KERNEL | KMEM_FLAG_WRITABLE));

  ASSERT_MSG(ret->pixel_bitmap, "Could not make bitmap!");

  /* Make sure it is cleared */
  memset(ret->window_stack, 0, ret->window_stack_size);

  return ret;
}

/*!
 * @brief: Destroy memory owned by the screen
 */
void destroy_lwnd_screen(lwnd_screen_t* screen)
{
  if (!screen)
    return;

  destroy_mutex(screen->draw_lock);
  destroy_mutex(screen->event_lock);
  destroy_bitmap(screen->pixel_bitmap);
  Must(__kmem_kernel_dealloc((uint64_t)screen->window_stack, screen->window_stack_size));
  deallocate_lwnd_screen(screen);
}

/*!
 * @brief: Register a window to the screen
 */
int lwnd_screen_register_window(lwnd_screen_t* screen, lwnd_window_t* window)
{
  mutex_lock(screen->draw_lock);

  window->screen = screen;

  screen->window_stack[screen->window_count] = window;
  window->id = screen->window_count;

  screen->window_count++;

  mutex_unlock(screen->draw_lock);
  return 0;
}

/*!
 * @brief: Register a window at a specific id
 *
 * We can only register at places where there either already is a window, or 
 * at the end of the current stack, resulting in a call to lwnd_screen_register_window
 *
 * FIXME: do we need to update windows anywhere?
 */
int lwnd_screen_register_window_at(lwnd_screen_t* screen, lwnd_window_t* window, window_id_t newid)
{
  lwnd_window_t* c, *p;

  if (newid >= screen->window_count)
    return -1;

  /* Register at the end? Just register */
  if (newid == screen->window_count-1)
    return lwnd_screen_register_window(screen, window);

  mutex_lock(screen->draw_lock);

  c = screen->window_stack[newid];
  p = c;

  /* Set the new window here */
  screen->window_stack[newid] = window;
  window->id = newid;

  newid++;
  c = screen->window_stack[newid];

  while (c) {

    /* Shift the previous window forward */
    screen->window_stack[newid] = p;
    screen->window_stack[newid]->id = newid;

    /* Cache the current entry as the previous */
    p = c;

    /* Cycle to the next window */
    newid++;
    c = screen->window_stack[newid];
  }

  screen->window_count++;

  mutex_unlock(screen->draw_lock);
  return 0;
}

/*!
 * @brief: Remove a window from a screen
 *
 * This shifts the windows after it back by one and updates the windowids
 */
int lwnd_screen_unregister_window(lwnd_screen_t* screen, lwnd_window_t* window)
{
  int error;

  if (!screen || !screen->window_count)
    return -1;

  if (window->id == LWND_INVALID_ID)
    return -1;

  /* Check both range and validity */
  if (!lwnd_window_has_valid_index(window))
    return -1;

  error = 0;

  mutex_lock(screen->draw_lock);

  /* We know window is valid and contained within the stack */
  for (uint32_t i = window->id; i < screen->window_count; i++) {

    if (i+1 < screen->window_count) {
      screen->window_stack[i] = screen->window_stack[i + 1];

      if (screen->window_stack[i]) {
        screen->window_stack[i]->id = i;
      }
    } else 
      screen->window_stack[i] = nullptr;
  }

  window->id = LWND_INVALID_ID;
  window->screen = nullptr;

  screen->window_count--;

  mutex_unlock(screen->draw_lock);

  return error;
}

/*!
 * @brief: Scan the entire window stack to find a window by label
 */
lwnd_window_t* lwnd_screen_get_window_by_label(lwnd_screen_t* screen, const char* label)
{
  lwnd_window_t* c;

  for (uint32_t i = 0; i < screen->window_count; i++) {
    c = screen->window_stack[i];

    if (!c || strcmp(c->label, label) == 0)
      break;

    c = nullptr;
  }

  return c;
}

/*!
 * @brief: Draw a windows framebuffer to the screen
 *
 * TODO: can we integrate 2D accelleration here to make this at least a little fast?
 */
int lwnd_screen_draw_window(lwnd_window_t* window)
{
  lwnd_screen_t* this;
  fb_color_t* current_color;

  if (!window)
    return -1;

  this = window->screen;

  uint32_t current_offset = this->info->pitch * window->y + window->x * this->info->bpp / 8;
  const uint32_t bpp = this->info->bpp;
  const uint32_t increment = (bpp / 8);

  for (uint32_t i = 0; i < window->height; i++) {
    for (uint32_t j = 0; j < window->width; j++) {

      if (window->x + j >= this->width)
        break;

      if (bitmap_isset(this->pixel_bitmap, (window->y + i) * this->width + (window->x + j)))
        continue;

      current_color = get_color_at(window, j, i);

      //if (this->info->colors.alpha.length_bits)
        //*(uint8_t volatile*)(this->info->kernel_addr + current_offset + j * increment + (this->info->colors.alpha.offset_bits / 8)) = current_color->components.a;

      *(uint8_t volatile*)(this->info->kernel_addr + current_offset + j * increment + (this->info->colors.red.offset_bits / 8)) = current_color->components.r;
      *(uint8_t volatile*)(this->info->kernel_addr + current_offset + j * increment + (this->info->colors.green.offset_bits / 8)) = current_color->components.g;
      *(uint8_t volatile*)(this->info->kernel_addr + current_offset + j * increment + (this->info->colors.blue.offset_bits / 8)) = current_color->components.b;

      bitmap_mark(this->pixel_bitmap, (window->y + i) * this->width + (window->x + j));
    }

    if (window->y + i >= this->height)
      break;

    current_offset += this->info->pitch;

  }

  return 0;
}

/*!
 * @brief: Add an lwnd_event to the end of a screens event chain
 *
 * Caller musn't take the ->event_lock of @screen
 */
int lwnd_screen_add_event(lwnd_screen_t* screen, lwnd_event_t* event)
{
  lwnd_event_t** current;

  current = &screen->events;

  mutex_lock(screen->event_lock);

  while (*current) {
    if (*current == event)
      break;

    current = &(*current)->next;
  }

  if (*current) {
    mutex_unlock(screen->event_lock);
    return -1;
  }

  *current = event;

  mutex_unlock(screen->event_lock);
  return 0;
}

/*!
 * @brief: Takes the head event from the screens event chain
 *
 * Caller musn't take the ->event_lock of @screen
 */
lwnd_event_t* lwnd_screen_take_event(lwnd_screen_t* screen)
{
  lwnd_event_t* ret;

  mutex_lock(screen->event_lock);

  ret = screen->events;

  if (!ret) {
    mutex_unlock(screen->event_lock);
    return nullptr;
  }

  screen->events = ret->next;

  mutex_unlock(screen->event_lock);
  return ret;
}

/*!
 * @brief: Handle a single event from the screen
 */
void lwnd_screen_handle_event(lwnd_screen_t* screen, lwnd_event_t* event)
{
  lwnd_window_t* window;

  switch (event->type) {
    case LWND_EVENT_TYPE_MOVE:
      {
        window = lwnd_screen_get_window(screen, event->window_id);

        if (!window)
          break;

        lwnd_window_move(window, event->move.new_x, event->move.new_y);
        break;
      }
    default:
      break;
  }
}

/*!
 * @brief: Handle every event in the screens chain
 *
 * TODO: event compositing
 */
void lwnd_screen_handle_events(lwnd_screen_t* screen)
{
  lwnd_event_t* current_event;
  lwnd_event_t* next_event;

  if (!screen || !screen->events)
    return;

  current_event = lwnd_screen_take_event(screen);

  while (current_event) {
    next_event = lwnd_screen_take_event(screen);

    /* Consecutive events acting on the same window? */
    while (next_event && next_event->type == current_event->type && next_event->window_id == current_event->window_id) {
      /* Destroy this one preemptively, since it does not really matter to us */
      destroy_lwnd_event(current_event);

      /* Set the current event to the next one */
      current_event = next_event;

      /* Cycle further */
      next_event = lwnd_screen_take_event(screen);
    }

    lwnd_screen_handle_event(screen, current_event);

    destroy_lwnd_event(current_event);

    current_event = next_event;
  }
}
