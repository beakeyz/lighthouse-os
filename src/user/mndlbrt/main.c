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
    float real;
    float imag;
};

static float compl_get_len_sq(struct compl *c)
{
    return (c->real * c->real) + (c->imag * c->imag);
}

static inline void compl_square(struct compl *z)
{
    float new_real = (z->real * z->real) - (z->imag * z->imag);
    float new_imag = 2 * (z->real * z->imag);

    z->real = new_real;
    z->imag = new_imag;
}

static inline void compl_scale(struct compl *z, float scalar)
{
    z->imag *= scalar;
    z->real *= scalar;
}

static inline void compl_add(struct compl *z, float scalar)
{
    z->imag += scalar;
    z->real += scalar;
}

static inline void compl_funct(struct compl *z)
{
    compl_square(z);
    // compl_square(z);

    z->imag -= 0.8f;
}

/* Local shit */
static lightui_window_t* window;
static struct compl offset_vec;
static struct compl origin_vec;
static float scale;
static lcolor_t* img;

#define EXIT_ERROR(str) (printf("[ERROR]: %s\n", str) - 1)

static inline void draw_mandlebrot()
{
    int n;
    lcolor_t* color = img;
    struct compl c;

    for (int y = 0; y < window->lui_height; y++) {
        for (int x = 0; x < window->lui_width; x++) {

            n = 0;
            c = (struct compl ) { (x + origin_vec.real) * scale + offset_vec.real, (y + origin_vec.imag) * scale + offset_vec.imag };

            while (n < 32) {
                /* Check if the coord is inside a circle with r=2 */
                if (fabsf(compl_get_len_sq(&c)) >= 4)
                    break;

                compl_funct(&c);
                n++;
            }

            *(color++) = LCLR_RGBA(n << 3, 0, 0, 0xff);
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
    origin_vec = (struct compl ) { -(window->lui_width / 2.0f), -(window->lui_height / 2.0f) };
    offset_vec = (struct compl ) { 0.0f, 0.0f };
    scale = 1.f / 1500.f;

    img_buffer.width = window->lui_width;
    img_buffer.height = window->lui_height;
    img_buffer.buffer = img;

    memset(img, 0, sizeof(*img) * window->lui_width * window->lui_height);

#define SCALE_UP_FACTOR 1.05f
#define SCALE_DOWN_FACTOR 0.95f

    do {

        if (get_key_event(&window->gfxwnd, &event) && event.pressed) {
            switch (event.keycode) {
            case ANIVA_SCANCODE_W:
                offset_vec.imag -= 10 * scale;
                break;
            case ANIVA_SCANCODE_A:
                offset_vec.real -= 10 * scale;
                break;
            case ANIVA_SCANCODE_S:
                offset_vec.imag += 10 * scale;
                break;
            case ANIVA_SCANCODE_D:
                offset_vec.real += 10 * scale;
                break;
            case ANIVA_SCANCODE_E:
                scale *= SCALE_UP_FACTOR;
                break;
            case ANIVA_SCANCODE_Q:
                scale *= SCALE_DOWN_FACTOR;
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
