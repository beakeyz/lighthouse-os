#include "button.h"
#include "libgfx/video.h"
#include "lightui/widget.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>

/*!
 * @brief: Do generic drawing for a button
 *
 * Buttons may (but don't need) contain a label which gets fitted into it's rectangle area. In case there
 * is a value in ->content, we use this to load the 'background image' for this button. The resource should
 * be an .ico file which may itself contain a bmp image format. If the width and hight of the image don't match
 * the dimentions of the widget, the image will be scaled to fit the dimentions.
 */
static void __generic_button_draw(lightui_widget_t* widget)
{
    // lightui_draw_rect(widget->wnd, widget->x, widget->y, widget->width, widget->height, LCLR_RGBA(0xff, 0, 0, 0xff));

    /*
     * Temprorary lmao
     * We need to be able to create solid colors for the close, minimize and maximize buttons xD
     */

    if (strncmp("red", widget->content, 3) == 0)
        lwindow_draw_rect(&widget->wnd->gfxwnd, widget->x, widget->y, widget->width, widget->height, LCLR_RGBA(0xff, 0, 0, 0xff));
    else if (strncmp("orange", widget->content, 6) == 0)
        lwindow_draw_rect(&widget->wnd->gfxwnd, widget->x, widget->y, widget->width, widget->height, LCLR_RGBA(0xfc, 0x75, 0, 0xff));
    else if (strncmp("green", widget->content, 5) == 0)
        lwindow_draw_rect(&widget->wnd->gfxwnd, widget->x, widget->y, widget->width, widget->height, LCLR_RGBA(0x00, 0xff, 0, 0xff));
    else
        lwindow_draw_rect(&widget->wnd->gfxwnd, widget->x, widget->y, widget->width, widget->height, LCLR_RGBA(0x66, 0x66, 0x66, 0xff));
}

int lightui_add_button_widget(lightui_window_t* wnd, const char* label, const char* image_path, uint32_t x, uint32_t y, uint32_t width, uint32_t height, f_lightui_widget_onevent_t onevent)
{
    lightui_widget_t* widget;

    if (!wnd)
        return -EINVAL;

    widget = malloc(sizeof(*widget));

    if (!widget)
        return -ENOMEM;

    memset(widget, 0, sizeof(*widget));

    widget->x = x;
    widget->y = y;
    widget->width = width;
    widget->height = height;
    widget->label = label;
    widget->content = image_path;
    widget->type = LIGHTUI_WIDGET_TYPE_BUTTON;

    /* Callback stuff */
    widget->f_onevent = onevent;
    widget->f_draw = __generic_button_draw;

    /* Add the widget to the window */
    return lightui_window_add_widget(wnd, widget);
}
