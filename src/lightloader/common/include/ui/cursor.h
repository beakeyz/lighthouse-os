#ifndef __LIGHTLOADER_UI_CURSOR__
#define __LIGHTLOADER_UI_CURSOR__

#include "gfx.h"
#include <image.h>

/*
 * Currently: one cursor sprite fitted in one bmp file
 *
 * How about: one big PNG file with multiple cursor sprites of
 * different stages, like loading, clickable, ect.
 */

void init_cursor(light_gfx_t* gfx);

void draw_cursor(light_gfx_t* gfx, uint32_t x, uint32_t y);
void update_cursor_pixel(light_gfx_t* gfx, uint32_t x, uint32_t y, uint32_t clr);

#endif // !__LIGHTLOADER_UI_CURSOR__
