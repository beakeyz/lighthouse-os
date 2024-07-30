#include "libgfx/shared.h"
#include "libgfx/video.h"
#include <libgfx/events.h>
#include <lightos/event/key.h>
#include <lightui/draw.h>
#include <lightui/window.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct compl
{
    float x;
    float y;
};

static float compl_get_len_sq(struct compl *c)
{
    return (c->x * c->x) + (c->y * c->y);
}

lightui_window_t* window;
struct compl offset_vec;
float scale;
lcolor_t* img;

#define EXIT_ERROR(str) (printf("[ERROR]: %s\n", str) - 1)

static inline void draw_mandlebrot()
{
    int n;
    lcolor_t* color = img;
    struct compl c;
    struct compl z;

    for (int y = 0; y < window->lui_height; y++) {
        for (int x = 0; x < window->lui_width; x++) {

            n = 0;
            c = (struct compl ) { (x + offset_vec.x) * scale, (y + offset_vec.y) * scale };
            z = c;

            while (n < 64) {
                z = (struct compl ) { (z.x * z.x) - (z.y * z.y) + c.x, 2 * (z.x * z.y) + c.y };

                /* Check if the coord is inside a circle with r=2 */
                if (fabsf(compl_get_len_sq(&z)) >= 4)
                    break;

                n++;
            }

            *(color++) = LCLR_RGBA(n * 4, 0, n * 4, 0xff);
        }
    }
}

int main()
{
    lkey_event_t event;
    lclr_buffer_t img_buffer;

    window = lightui_request_window("mandlebrot", 512, 512, NULL);

    if (!window)
        return EXIT_ERROR("Failed to create window");

    img = malloc(sizeof(*img) * window->lui_width * window->lui_height);
    offset_vec = (struct compl ) { -(window->lui_width / 2.0f), -(window->lui_height / 2.0f) };
    scale = 0.05f / 1.5f;

    img_buffer.width = window->lui_width;
    img_buffer.height = window->lui_height;
    img_buffer.buffer = img;

    memset(img, 0, sizeof(*img) * window->lui_width * window->lui_height);

    do {

        if (get_key_event(&window->gfxwnd, &event) && event.pressed) {
            switch (event.keycode) {
            case ANIVA_SCANCODE_W:
                offset_vec.y -= 1.0f;
                break;
            case ANIVA_SCANCODE_A:
                offset_vec.x -= 1.0f;
                break;
            case ANIVA_SCANCODE_S:
                offset_vec.y += 1.0f;
                break;
            case ANIVA_SCANCODE_D:
                offset_vec.x += (1.0f);
                break;
            case ANIVA_SCANCODE_E:
                scale += 0.0005F;
                break;
            case ANIVA_SCANCODE_Q:
                scale -= 0.0005F;
                break;
            }
        }

        draw_mandlebrot();

        lightui_draw_buffer(window, 0, 0, &img_buffer);

        lightui_window_update(window);

        get_key_event(&window->gfxwnd, NULL);

        usleep(1);
    } while (true);

    free(img);
    return 0;
}
