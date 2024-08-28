#include "widget.h"
#include "lightui/window.h"

int lightui_window_add_widget(lightui_window_t* wnd, lightui_widget_t* widget)
{
    /* Link the widget into the windows widget chain */
    widget->next = wnd->widgets;
    wnd->widgets = widget;

    /* Set the widgets parent field */
    widget->wnd = wnd;

    /* Request a widget update */
    lightui_window_request_widget_update(wnd);
    return 0;
}
