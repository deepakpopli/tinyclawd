#include "canvas.h"

#include <math.h>
#include <stdlib.h>

static inline int iround(float v) { return (int)floorf(v + 0.5f); }

void Canvas::hline(int x0, int x1, int y, bool on) {
    if ((unsigned)y >= H) return;
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    if (x0 < 0) x0 = 0;
    if (x1 >= W) x1 = W - 1;
    uint8_t m = 1 << (y & 7);
    uint8_t* row = buf[y >> 3];
    for (int x = x0; x <= x1; x++) {
        if (on) row[x] |= m;
        else row[x] &= ~m;
    }
}

void Canvas::vline(int x, int y0, int y1, bool on) {
    if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
    for (int y = y0; y <= y1; y++) px(x, y, on);
}

void Canvas::fill_rect(int x0, int y0, int x1, int y1, bool on) {
    if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
    if (y0 < 0) y0 = 0;
    if (y1 >= H) y1 = H - 1;
    for (int y = y0; y <= y1; y++) hline(x0, x1, y, on);
}

void Canvas::rect(int x0, int y0, int x1, int y1, bool on) {
    hline(x0, x1, y0, on);
    hline(x0, x1, y1, on);
    vline(x0, y0, y1, on);
    vline(x1, y0, y1, on);
}

static void dot(Canvas& c, int x, int y, int width, bool on) {
    if (width <= 1) { c.px(x, y, on); return; }
    if (width == 2) { c.fill_rect(x, y, x + 1, y + 1, on); return; }
    float r = width / 2.0f;
    c.fill_ellipse(x - r + 0.5f, y - r + 0.5f, x + r - 0.5f, y + r - 0.5f, on);
}

void Canvas::line(float fx0, float fy0, float fx1, float fy1, int width, bool on) {
    int x0 = iround(fx0), y0 = iround(fy0), x1 = iround(fx1), y1 = iround(fy1);
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        dot(*this, x0, y0, width, on);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void Canvas::polyline(const float* p, int n, int width, bool on) {
    for (int i = 0; i + 1 < n; i++) line(p[2 * i], p[2 * i + 1], p[2 * i + 2], p[2 * i + 3], width, on);
}

void Canvas::fill_poly(const float* p, int n, bool on) {
    if (n < 3) return;
    float ymin = p[1], ymax = p[1];
    for (int i = 1; i < n; i++) {
        if (p[2 * i + 1] < ymin) ymin = p[2 * i + 1];
        if (p[2 * i + 1] > ymax) ymax = p[2 * i + 1];
    }
    int ya = (int)ceilf(ymin - 0.5f), yb = (int)floorf(ymax + 0.5f);
    if (ya < 0) ya = 0;
    if (yb >= H) yb = H - 1;
    float xs[16];
    for (int y = ya; y <= yb; y++) {
        float sy = y;
        int k = 0;
        for (int i = 0; i < n && k < 16; i++) {
            float x0 = p[2 * i], y0 = p[2 * i + 1];
            float x1 = p[2 * ((i + 1) % n)], y1 = p[2 * ((i + 1) % n) + 1];
            if ((y0 <= sy && y1 > sy) || (y1 <= sy && y0 > sy))
                xs[k++] = x0 + (sy - y0) * (x1 - x0) / (y1 - y0);
        }
        for (int i = 1; i < k; i++)
            for (int j = i; j > 0 && xs[j - 1] > xs[j]; j--) { float t = xs[j]; xs[j] = xs[j - 1]; xs[j - 1] = t; }
        for (int i = 0; i + 1 < k; i += 2) hline(iround(xs[i]), iround(xs[i + 1]), y, on);
    }
    // edges too, so thin slivers don't vanish
    for (int i = 0; i < n; i++)
        line(p[2 * i], p[2 * i + 1], p[2 * ((i + 1) % n)], p[2 * ((i + 1) % n) + 1], 1, on);
}

void Canvas::fill_ellipse(float x0, float y0, float x1, float y1, bool on) {
    float cx = (x0 + x1) / 2, cy = (y0 + y1) / 2;
    float rx = (x1 - x0) / 2 + 0.5f, ry = (y1 - y0) / 2 + 0.5f;
    if (rx <= 0 || ry <= 0) return;
    int ya = iround(y0), yb = iround(y1);
    for (int y = ya; y <= yb; y++) {
        float dy = (y - cy) / ry;
        float k = 1 - dy * dy;
        if (k < 0) continue;
        float hw = rx * sqrtf(k);
        hline(iround(cx - hw + 0.3f), iround(cx + hw - 0.3f), y, on);
    }
}

void Canvas::ellipse(float x0, float y0, float x1, float y1, int width, bool on) {
    arc(x0, y0, x1, y1, 0, 360, width, on);
}

void Canvas::arc(float x0, float y0, float x1, float y1, float a0, float a1, int width, bool on) {
    float cx = (x0 + x1) / 2, cy = (y0 + y1) / 2;
    float rx = (x1 - x0) / 2, ry = (y1 - y0) / 2;
    if (a1 < a0) a1 += 360;
    int steps = 8 + (int)((a1 - a0) / 360.0f * (rx + ry) * 4);
    // rotate one small step at a time instead of sin/cos per point,
    // the C3 has no fpu and a big ellipse used to cost milliseconds
    float da = (a1 - a0) / steps * (float)M_PI / 180.0f;
    float sc = cosf(da), ss = sinf(da);
    float ca = cosf(a0 * (float)M_PI / 180.0f), sa = sinf(a0 * (float)M_PI / 180.0f);
    float px_ = 0, py_ = 0;
    for (int i = 0; i <= steps; i++) {
        float x = cx + rx * ca, y = cy + ry * sa;
        if (i) line(px_, py_, x, y, 1, on);
        px_ = x;
        py_ = y;
        float t = ca * sc - sa * ss;
        sa = sa * sc + ca * ss;
        ca = t;
    }
    for (int w = 1; w < width; w++) arc(x0 + w, y0 + w, x1 - w, y1 - w, a0, a1, 1, on);
}

void Canvas::pieslice(float x0, float y0, float x1, float y1, float a0, float a1, bool on) {
    float cx = (x0 + x1) / 2, cy = (y0 + y1) / 2;
    float rx = (x1 - x0) / 2, ry = (y1 - y0) / 2;
    if (a1 < a0) a1 += 360;
    float pts[2 * 16];
    pts[0] = cx;
    pts[1] = cy;
    for (int i = 0; i <= 14; i++) {
        float a = (a0 + (a1 - a0) * i / 14) * (float)M_PI / 180.0f;
        pts[2 + 2 * i] = cx + rx * cosf(a);
        pts[3 + 2 * i] = cy + ry * sinf(a);
    }
    fill_poly(pts, 16, on);
}

void Canvas::blit(int x, int y, int w, int h, const uint8_t* bits, bool on) {
    int stride = (w + 7) / 8;
    for (int r = 0; r < h; r++) {
        const uint8_t* row = bits + r * stride;
        for (int c = 0; c < w; c++)
            if (row[c >> 3] & (0x80 >> (c & 7))) px(x + c, y + r, on);
    }
}

uint16_t utf8_next(const char*& s) {
    uint8_t c = *s++;
    if (c < 0x80) return c;
    if ((c & 0xe0) == 0xc0) {
        uint16_t v = (c & 0x1f) << 6;
        if (*s) v |= (*s++ & 0x3f);
        return v;
    }
    if ((c & 0xf0) == 0xe0) {
        uint16_t v = (c & 0x0f) << 12;
        if (*s) v |= (*s++ & 0x3f) << 6;
        if (*s) v |= (*s++ & 0x3f);
        return v;
    }
    while ((*s & 0xc0) == 0x80) s++;  // 4 byte, not supported
    return '?';
}

static const Glyph* find(const Font& f, uint16_t code) {
    for (int i = 0; i < f.count; i++)
        if (f.glyphs[i].code == code) return &f.glyphs[i];
    return nullptr;
}

int Canvas::glyph(int x, int y, uint16_t code, const Font& f, bool on) {
    const Glyph* g = find(f, code);
    if (!g) return x;
    blit(x, y, g->w, f.h, f.data + g->off, on);
    return x + g->adv;
}

int Canvas::text(int x, int y, const char* s, const Font& f, bool on) {
    while (*s) x = glyph(x, y, utf8_next(s), f, on);
    return x;
}

int Canvas::text_width(const char* s, const Font& f) const {
    int w = 0;
    while (*s) {
        const Glyph* g = find(f, utf8_next(s));
        if (g) w += g->adv;
    }
    return w;
}
