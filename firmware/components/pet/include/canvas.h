#pragma once
#include <stdint.h>
#include <string.h>

#include "fonts.h"

// 1-bit 128x64 framebuffer, stored the way the SH1106 wants it:
// 8 pages of 128 bytes, each byte is a column of 8 pixels (bit 0 = top).
// all rect/ellipse boxes are inclusive like PIL, so eyes.py ports 1:1.
class Canvas {
public:
    static const int W = 128;
    static const int H = 64;
    static const int PAGES = 8;

    uint8_t buf[PAGES][W];

    void clear() { memset(buf, 0, sizeof(buf)); }
    void fill() { memset(buf, 0xff, sizeof(buf)); }

    void px(int x, int y, bool on = true) {
        if ((unsigned)x >= W || (unsigned)y >= H) return;
        if (on) buf[y >> 3][x] |= 1 << (y & 7);
        else buf[y >> 3][x] &= ~(1 << (y & 7));
    }
    bool get(int x, int y) const {
        if ((unsigned)x >= W || (unsigned)y >= H) return false;
        return buf[y >> 3][x] & (1 << (y & 7));
    }

    void hline(int x0, int x1, int y, bool on = true);
    void vline(int x, int y0, int y1, bool on = true);
    void fill_rect(int x0, int y0, int x1, int y1, bool on = true);
    void rect(int x0, int y0, int x1, int y1, bool on = true);
    void line(float x0, float y0, float x1, float y1, int width = 1, bool on = true);
    void polyline(const float* pts, int n, int width = 1, bool on = true);  // pts = x,y pairs
    void fill_poly(const float* pts, int n, bool on = true);
    void fill_ellipse(float x0, float y0, float x1, float y1, bool on = true);
    void ellipse(float x0, float y0, float x1, float y1, int width = 1, bool on = true);
    void fill_circle(float cx, float cy, float r, bool on = true) { fill_ellipse(cx - r, cy - r, cx + r, cy + r, on); }
    // degrees, 0 = right (3 o'clock), clockwise like PIL
    void arc(float x0, float y0, float x1, float y1, float a0, float a1, int width = 1, bool on = true);
    void pieslice(float x0, float y0, float x1, float y1, float a0, float a1, bool on = true);

    // row-major msb-first bitmap, only set bits are drawn (transparent)
    void blit(int x, int y, int w, int h, const uint8_t* bits, bool on = true);

    // text: returns the x after the last glyph
    int text(int x, int y, const char* s, const Font& f = font_5x7, bool on = true);
    int text_width(const char* s, const Font& f = font_5x7) const;
    int text_center(int cx, int y, const char* s, const Font& f = font_5x7, bool on = true) {
        return text(cx - text_width(s, f) / 2, y, s, f, on);
    }
    // one glyph by unicode code point (for spinner / notes)
    int glyph(int x, int y, uint16_t code, const Font& f, bool on = true);

    void invert() {
        for (int p = 0; p < PAGES; p++)
            for (int x = 0; x < W; x++) buf[p][x] = ~buf[p][x];
    }
};

// decode one utf-8 code point, advances s
uint16_t utf8_next(const char*& s);
