#pragma once
#include <stdint.h>

struct Glyph {
    uint16_t code;   // unicode
    uint16_t off;    // into data
    uint8_t w;       // bitmap width
    uint8_t adv;     // advance
};

struct Font {
    uint8_t h;
    uint8_t count;
    const Glyph* glyphs;
    const uint8_t* data;  // row-major, msb first, (w + 7) / 8 bytes per row
};

extern const Font font_4x6, font_5x7, font_6x10;
extern const Font font_bold12, font_bold16, font_bold24;
extern const Font font_sym14, font_sym12, font_z;
