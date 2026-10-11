// usage: tired, limit hit, the status screen and the context meter
#include <stdio.h>

#include "anim.h"

static const float PI = (float)M_PI;

static float blink_k(uint32_t t, uint32_t at) {
    return t >= at && t < at + BLINK_MS ? (t - at) / (float)BLINK_MS : -1;
}

// "2h15", "40m" or "--"
static void fmt_reset(char* out, int cap, int min) {
    if (min < 0) snprintf(out, cap, "--");
    else if (min < 60) snprintf(out, cap, "%dm", min);
    else snprintf(out, cap, "%dh%02d", min / 60, min % 60);
}

// --------------------------------------------------------------------- tired

static void a_tired(Canvas& c, uint32_t t, const Ctx& x) {
    uint32_t slot = t / 3000, in = t % 3000;
    uint32_t h = hash32(slot * 23 + x.seed);
    Eye e;
    e.lid = 0.5f;   // same as the HALF shape, but it can blink
    e.dy = 2 + breathe(t, 2200);
    // slow heavy blink
    uint32_t b = 300 + h % 1500;
    if (in >= b && in < b + 900) e.lid = 0.5f + 0.5f * sinf(PI * seg(in, b, b + 900));
    // a sigh now and then
    if ((h >> 10) % 3 == 0 && in >= 1800 && in < 2900) e.dy += 2 * sinf(PI * seg(in, 1800, 2900));
    face(c, e);
    // sweat drop runs down one side
    uint32_t s = (h >> 4) % 900;
    if (in >= s && in < s + 1800) {
        float sx = (h >> 14) & 1 ? 103 : 22;
        sweat(c, sx, 14 + 20 * ease_in(seg(in, s, s + 1800)));
    }
}

// ----------------------------------------------------------------- limit hit

static void a_limit_hit(Canvas& c, uint32_t t, const Ctx& x) {
    float amp = 0.6f + 0.4f * fabsf(sinf(t / 2900.0f));
    Eye e;
    e.shape = CROSS;
    e.dx = 3 * amp * sinf(t / 80.0f);
    e.dy = 1 + sinf(t / 1300.0f);
    face(c, e);
    char r[12], line[24];
    fmt_reset(r, sizeof(r), x.reset_min);
    if (x.reset_min >= 0) snprintf(line, sizeof(line), "back in %s", r);
    else snprintf(line, sizeof(line), "limit hit");
    if ((t / 2000) % 4 != 3) c.text_center(64, 56, line, font_4x6);
    else c.text_center(64, 56, "limit hit", font_4x6);
    if ((t % 4500) < 1500) stars(c, 64 + e.dx, 8, t / 800.0f);
}

// --------------------------------------------------------------------- usage

static void bar(Canvas& c, int y, const char* label, int pct, float k) {
    char s[16];
    if (pct < 0) snprintf(s, sizeof(s), "%s  --", label);
    else snprintf(s, sizeof(s), "%s  %d%%", label, iround(pct * k));
    c.text(44, y, s, font_5x7);
    c.rect(44, y + 10, 123, y + 16);
    if (pct < 0) {
        for (int xx = 47; xx < 121; xx += 4) c.px(xx, y + 13);
        return;
    }
    int w = iround(75 * clamp01(pct / 100.0f) * k);
    if (w > 0) c.fill_rect(46, y + 12, 46 + w - 1, y + 14);
}

static void a_usage(Canvas& c, uint32_t t, const Ctx& x) {
    float k1 = ease_out(seg(t, 150, 1250));
    float k2 = ease_out(seg(t, 350, 1450));
    bar(c, 1, "5h", x.pct5h, k1);
    bar(c, 22, "wk", x.pctwk, k2);
    char r[12], s[24];
    fmt_reset(r, sizeof(r), x.reset_min);
    snprintf(s, sizeof(s), x.reset_min >= 0 ? "resets in %s" : "resets %s", r);
    c.text(44, 50, s, font_5x7);
    // small eyes on the left read the bars, top then bottom then you
    const Key gaze[] = {{0, 0}, {300, -5}, {1300, -5}, {1600, 3}, {2600, 3}, {2900, 0}};
    Eye e;
    e.w = 7;
    e.h = 14;
    e.dy = KEYS(gaze, t % 6000);
    e.dx = (t % 6000) < 2900 ? 2 : 0;
    uint32_t slot = t / 3000, h = hash32(slot + x.seed);
    float bk = blink_k(t % 3000, 800 + h % 1800);
    if (bk >= 0) e = blink_eye(e, bk);
    Eye r2 = e;
    r2.mirror = true;
    draw_eye(c, 12, 30, e);
    draw_eye(c, 28, 30, r2);
}

// ------------------------------------------------------------------- context

static void a_context(Canvas& c, uint32_t t, const Ctx& x) {
    const int X0 = 92, X1 = 120, Y0 = 9, Y1 = 58;
    int pct = x.ctx;
    float k = ease_out(seg(t, 200, 1400));
    float level = pct < 0 ? 0 : clamp01(pct / 100.0f) * k;
    // jar
    c.rect(X0, Y0, X1, Y1);
    c.fill_rect(X0 + 7, Y0 - 5, X1 - 7, Y0 - 1);
    int inner = Y1 - Y0 - 3;
    float top = Y1 - 2 - inner * level;
    if (level > 0) {
        for (int xx = X0 + 2; xx <= X1 - 2; xx++) {
            float wy = top + (level < 0.99f ? sinf(xx / 3.0f + t / 220.0f) : 0);
            int y0 = iround(wy);
            if (y0 < Y0 + 2) y0 = Y0 + 2;
            c.vline(xx, y0, Y1 - 2);
        }
    }
    bool full = pct > 80;
    char s[16];
    if (pct < 0) {
        snprintf(s, sizeof(s), "ctx --");
        c.text_center((X0 + X1) / 2 + 1, 28, "?", font_6x10);
    } else snprintf(s, sizeof(s), "ctx %d%%", iround(pct * k));
    c.text_center(46, 56, s, font_5x7);
    // eyes look at the jar, at the level
    Eye e;
    e.w = 11;
    e.h = 20;
    e.dx = 4;
    e.dy = clampf((top - 30) / 4, -4, 5) - 4;
    if (full && k > 0.8f) {
        e.shape = SAD;
        e.dy = -2;
    }
    uint32_t slot = t / 2700, h = hash32(slot * 7 + x.seed);
    float bk = blink_k(t % 2700, 700 + h % 1600);
    if (bk >= 0 && e.shape == RECT) e = blink_eye(e, bk);
    Eye r = e;
    r.mirror = true;
    draw_eye(c, 30, 28, e);
    draw_eye(c, 62, 28, r);
    if (full && k > 0.8f) {
        uint32_t q = t % 2000;
        if (q < 1400) sweat(c, 80, 6 + 18 * ease_in(q / 1400.0f));
        if ((t / 300) % 2) c.hline(X0 + 2, X1 - 2, Y0 + 2);   // brim flashing
    }
}

static const Anim A_TIRED = {"tired", a_tired, 6000, LOOP};
static const Anim A_LIMIT = {"limit_hit", a_limit_hit, 8000, LOOP};
static const Anim A_USAGE = {"usage", a_usage, 6000, LOOP | SCENE};
static const Anim A_CONTEXT = {"context", a_context, 5400, LOOP | SCENE};

ANIM_LIST(anims_usage, &A_TIRED, &A_LIMIT, &A_USAGE, &A_CONTEXT);
