#include "dev/video/framebuffer.h"
#include "draw.h"
#include "libk/flow/error.h"

int lwnd_draw_dbg_box(lwnd_screen_t* screen, u32 x, u32 y, u32 w, u32 h, fb_color_t clr)
{
    fb_info_t* info;

    if (!screen)
        return -KERR_INVAL;

    info = screen->fbinfo;

    if (!info)
        return -KERR_INVAL;

    generic_draw_rect(info, x, y, w, 1, clr);
    generic_draw_rect(info, x, y, 1, h, clr);
    generic_draw_rect(info, x + w - 1, y, 1, h, clr);
    generic_draw_rect(info, x, y + h - 1, w, 1, clr);
    return 0;
}
