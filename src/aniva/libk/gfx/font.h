#ifndef __ANIVA_FONT__
#define __ANIVA_FONT__

#include <libk/stddef.h>

/*
 * Very incomplete font structure
 */
typedef struct aniva_font {
    /* Width and Height, counted in pixels */
    uint8_t width, height;
    uint16_t bytes_per_glyph;
    uint32_t glyph_count;
    uint8_t data[];
} aniva_font_t;

#define CURSOR_GLYPH 127

int get_glyph_at(aniva_font_t* font, uint32_t index, uint8_t** out);
int get_glyph_for_char(aniva_font_t* font, char c, uint8_t** out);

void get_default_font(aniva_font_t** font);

#endif // !__ANIVA_FONT__
