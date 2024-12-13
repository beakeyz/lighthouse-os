#ifndef __LIGHTOS_LIGHTUI_BUTTON_WIDGET_H__
#define __LIGHTOS_LIGHTUI_BUTTON_WIDGET_H__

#include <lightos/lightos.h>
#include "lightui/widget.h"
#include "lightui/window.h"

LEXPORT int lightui_add_button_widget(lightui_window_t* wnd, const char* label, const char* image_path, uint32_t x, uint32_t y, uint32_t width, uint32_t height, f_lightui_widget_onevent_t onevent);

#endif // !__LIGHTOS_LIGHTUI_BUTTON_WIDGET_H__
