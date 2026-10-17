#include "face.h"

#include "motion.h"

static void thick(Canvas& c, const float* p, int n) { c.polyline(p, n, 3); }

void draw_eye(Canvas& c, float cx, float cy, const Eye& e) {
    float x = cx + e.dx, y = cy + e.dy;
    float w = e.w < 1 ? 1 : e.w, h = e.h < 2 ? 2 : e.h;
    int s = e.mirror ? -1 : 1;
    switch (e.shape) {
    case RECT: {
        float top = y - h / 2 + e.lid * h;
        float bot = y + h / 2 - 1;
        if (bot - top < 1) top = bot - 1;   // closed: a 2 px line
        c.fill_rect(iround(x - w / 2), iround(top), iround(x + w / 2 - 1), iround(bot));
        break;
    }
    case HALF:
        c.fill_rect(iround(x - w / 2), iround(y), iround(x + w / 2 - 1), iround(y + h / 2 - 1));
        break;
    case HAPPY: {
        float p[] = {x - w / 2 - 2, y + 4, x, y - 6, x + w / 2 + 2, y + 4};
        thick(c, p, 3);
        break;
    }
    case SQUINT: {
        float p[] = {x - s * 6.f, y - 8, x + s * 6.f, y, x - s * 6.f, y + 8};
        thick(c, p, 3);
        break;
    }
    case CROSS:
        c.line(x - 7, y - 7, x + 7, y + 7, 3);
        c.line(x - 7, y + 7, x + 7, y - 7, 3);
        break;
    case HEART:
        heart(c, x, y, e.w / EYE_W);
        break;
    case SAD:
    case ANGRY: {
        // slanted top edge: sad = inner corner up, angry = inner corner down
        bool inner_up = e.shape == SAD;
        float top = y - h / 2, bot = y + h / 2 - 1;
        float hi = top, lo = top + 8;
        bool flip = inner_up != e.mirror;
        float lt = flip ? lo : hi, rt = flip ? hi : lo;
        float p[] = {x - w / 2, lt, x + w / 2 - 1, rt, x + w / 2 - 1, bot, x - w / 2, bot};
        c.fill_poly(p, 4);
        break;
    }
    case SPIRAL: {
        float px_ = 0, py_ = 0;
        for (int i = 0; i < 60; i++) {
            float t = i / 59.0f * 3 * (float)M_PI;
            float r = 1 + 2.3f * t;
            float a = t + e.phase * s;
            float qx = x + r * cosf(a) * 0.9f, qy = y + r * sinf(a) * 0.9f;
            if (i) c.line(px_, py_, qx, qy, 2);
            px_ = qx;
            py_ = qy;
        }
        break;
    }
    }
}

void face(Canvas& c, const Eye& l, const Eye& r) {
    draw_eye(c, LEFT_X, EYE_Y, l);
    draw_eye(c, RIGHT_X, EYE_Y, r);
}

void face(Canvas& c, const Eye& e) {
    Eye r = e;
    r.mirror = true;
    face(c, e, r);
}

Eye blink_eye(const Eye& base, float k) {
    // same shape as eyes.py: 14, 6, 2, 2, 6, 14, 22 with width +1..+3
    static const float hs[] = {22, 14, 6, 2, 2, 6, 14, 22};
    static const float gs[] = {0, 1, 2, 3, 3, 2, 1, 0};
    float i = clamp01(k) * 7;
    int i0 = (int)i;
    if (i0 >= 7) i0 = 6;
    float f = i - i0;
    Eye e = base;
    e.h = lerp(hs[i0], hs[i0 + 1], f) * base.h / EYE_H;
    e.w = base.w + lerp(gs[i0], gs[i0 + 1], f);
    return e;
}

void face_blink(Canvas& c, uint32_t t, uint32_t at, const Eye& base) {
    if (t >= at && t < at + BLINK_MS) face(c, blink_eye(base, (t - at) / (float)BLINK_MS));
    else face(c, base);
}

float breathe(uint32_t t, uint32_t period) {
    float p = (t % period) / (float)period;
    return (p > 0.35f && p < 0.65f) ? 1 : 0;
}

void heart(Canvas& c, float x, float y, float k) {
    float r = 4 * k;
    c.fill_ellipse(x - 2 * r, y - r * 1.5f, x, y + r * 0.5f);
    c.fill_ellipse(x, y - r * 1.5f, x + 2 * r, y + r * 0.5f);
    float p[] = {x - 2 * r, y - r * 0.4f, x + 2 * r, y - r * 0.4f, x, y + r * 2.2f};
    c.fill_poly(p, 3);
}

void sparkle(Canvas& c, int x, int y, int r) {
    c.hline(x - r, x + r, y);
    c.vline(x, y - r, y + r);
}

void sweat(Canvas& c, float x, float y) {
    float p[] = {x, y, x - 3, y + 6, x + 3, y + 6};
    c.fill_poly(p, 3);
    c.fill_ellipse(x - 3, y + 3, x + 3, y + 9);
}

void tear(Canvas& c, float x, float y) {
    c.fill_ellipse(x - 2, y - 2, x + 2, y + 2);
    c.px(iround(x), iround(y - 3));
}

void ball(Canvas& c, float x, float y, float r) {
    c.fill_ellipse(x - r, y - r, x + r, y + r);
    if (r >= 4) {   // panel pattern on a big ball
        c.fill_rect(iround(x - 1), iround(y - 1), iround(x + 1), iround(y + 1), false);
        for (int a = 0; a < 360; a += 72) {
            float rad = (a - 90) * (float)M_PI / 180;
            c.px(iround(x + 0.7f * r * cosf(rad)), iround(y + 0.7f * r * sinf(rad)), false);
        }
    } else {
        c.px(iround(x), iround(y), false);
    }
}

void zzz(Canvas& c, float t) {
    // three z's drifting up and right, small to big
    static const int x0s[] = {98, 104, 110}, y0s[] = {36, 22, 8};
    static const Font* fonts[] = {&font_6x10, &font_z, &font_bold16};
    for (int k = 0; k < 3; k++) {
        float p = fmodf(t + k * 0.33f, 1.0f);
        if (p < 0.85f) c.text(iround(x0s[k] + 4 * p), iround(y0s[k] - 6 * p), k == 2 ? "Z" : "z", *fonts[k]);
    }
}

void stars(Canvas& c, float x, float y, float t) {
    for (int k = 0; k < 3; k++) {
        float a = t * 2 * (float)M_PI + k * 2 * (float)M_PI / 3;
        sparkle(c, iround(x + 12 * cosf(a)), iround(y + 4 * sinf(a)), 2);
    }
}
