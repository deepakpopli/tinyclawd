// usage: tired, limit hit, the status screen and the context meter
#include <stdio.h>

#include "anim.h"
#include "icons.h"

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

// little marks in front of the labels
static const char* const HOURGLASS[] = {"#####", "#...#", " #.# ", "  #  ", " #.# ", "#.#.#", "#####"};
static const char* const CALENDAR[] = {" # # ", "#####", "#####", "#...#", "#.#.#", "#...#", "#####"};
static const char* const CLOCK[] = {" ### ", "#.#.#", "#.#.#", "#..##", " ### "};

// one row: mark and label left, percent right, a round ended bar under it
static void usage_row(Canvas& c, int y, const char* const* mark, const char* label, int pct, float k) {
    char s[8];
    art(c, 0, y + 1, mark, 7);
    c.text(8, y, label, font_6x10);
    if (pct < 0) snprintf(s, sizeof(s), "--");
    else snprintf(s, sizeof(s), "%d%%", iround((pct > 100 ? 100 : pct) * k));
    c.text(128 - c.text_width(s, font_6x10), y, s, font_6x10);
    c.rect(0, y + 11, 127, y + 19);
    c.px(0, y + 11, false);
    c.px(127, y + 11, false);
    c.px(0, y + 19, false);
    c.px(127, y + 19, false);
    for (int q = 1; q < 4; q++) c.px(q * 32, y + 21);   // quarter marks
    if (pct > 0) {
        int w = iround(124 * (pct > 100 ? 1 : pct / 100.0f) * k);
        if (w > 0) c.fill_rect(2, y + 13, 1 + w, y + 17);
        if (w > 2) {   // round the end of the fill too
            c.px(1 + w, y + 13, false);
            c.px(1 + w, y + 17, false);
        }
    }
}

// only the numbers, the screen is small
static void a_usage(Canvas& c, uint32_t t, const Ctx& x) {
    usage_row(c, 0, HOURGLASS, "5 hour", x.pct5h, ease_out(seg(t, 100, 900)));
    usage_row(c, 24, CALENDAR, "week", x.pctwk, ease_out(seg(t, 250, 1050)));
    char r[12], s[24];
    fmt_reset(r, sizeof(r), x.reset_min);
    snprintf(s, sizeof(s), "resets in %s", r);
    if (x.reset_min >= 0) {
        int w = c.text_width(s, font_5x7) + 8;
        art(c, 64 - w / 2, 56, CLOCK, 5);
        c.text(64 - w / 2 + 8, 55, s, font_5x7);
    }
}

// ------------------------------------------------------------------- context

// a box getting stuffed with pages. up to 80% they fit, past that they stick
// out over the top, the lid gets pushed up and he strains
static void a_context(Canvas& c, uint32_t t, const Ctx& x) {
    const int X0 = 80, X1 = 122, BOT = 61, TOP = 22;   // the box
    const int ROW = 5;
    int pct = x.ctx < 0 ? 0 : (x.ctx > 100 ? 100 : x.ctx);
    int rows = (pct + 5) / 10;   // 8 rows fill the box, 9 and 10 stick out
    const uint32_t DROP = 160, GAP = 110;
    uint32_t done = rows * (DROP + GAP);
    int shown = t >= done ? rows : t / (DROP + GAP);
    float fall = t >= done ? 1 : seg(t % (DROP + GAP), 0, DROP);
    bool over = rows > 8 && t >= done;
    float press = over ? ((t / 140) % 2 ? 1 : 0) : 0;   // the lid jumps on the pile

    // the box: walls bulge out when it is too full
    int bulge = over ? 2 : 0;
    float lw[] = {(float)X0, (float)TOP, (float)X0 - bulge, (TOP + BOT) / 2.f, (float)X0, (float)BOT};
    float rw[] = {(float)X1, (float)TOP, (float)X1 + bulge, (TOP + BOT) / 2.f, (float)X1, (float)BOT};
    c.polyline(lw, 3, 2);
    c.polyline(rw, 3, 2);
    c.line(X0, BOT, X1, BOT, 2);

    // pages: rows of blocks with gaps, the newest one falls in
    int top_y = BOT - 2;
    for (int r = 0; r <= shown && r < rows; r++) {
        float k = r < shown ? 1 : fall;
        if (r == shown && t >= done) break;
        int y1 = BOT - 2 - r * ROW;
        int y = iround(lerp(0, y1, ease_in(k)));
        uint32_t h = hash32(r * 31 + x.seed);
        int xx = X0 + 3 + (int)(h % 3);
        while (xx < X1 - 4) {
            int w = 5 + (int)((h >>= 3) % 12);
            if (xx + w > X1 - 3) w = X1 - 3 - xx;
            c.fill_rect(xx, y - ROW + 2, xx + w, y);
            xx += w + 2;
        }
        if (k >= 1) top_y = y - ROW + 2;
    }

    // the lid: sits on the box, or gets pushed up and tilted by the pile
    float lid_y = top_y < TOP ? top_y - 2 - press : TOP - 3;
    if (t < done + 200) lid_y = lerp(-4, lid_y, ease_in(seg(t, done, done + 200)));
    float tilt = top_y < TOP ? 5 : 0;
    float lid[] = {X0 - 3.f, lid_y + tilt, X1 + 3.f, lid_y - tilt, X1 + 3.f, lid_y - tilt - 3, X0 - 3.f, lid_y + tilt - 3};
    c.fill_poly(lid, 4);

    char s[20];
    snprintf(s, sizeof(s), x.ctx < 0 ? "context --" : "context %d%%", pct);
    c.text_center(38, 56, s, font_5x7);

    // eyes watch the box. they strain as it fills, and squeeze shut when it overflows
    Eye e;
    e.w = 11;
    e.h = pct > 60 ? 20 - (pct - 60) / 4 : 20;
    e.dx = 4;
    e.dy = -4;
    if (over) {
        e.shape = SQUINT;
        e.dy = -6 + press;
    } else {
        uint32_t slot = t / 2700, h = hash32(slot * 7 + x.seed);
        float bk = blink_k(t % 2700, 700 + h % 1600);
        if (bk >= 0) e = blink_eye(e, bk);
    }
    Eye r = e;
    r.mirror = true;
    draw_eye(c, 24, 28, e);
    draw_eye(c, 54, 28, r);
    if (pct > 80 && t >= done) {   // sweating it
        uint32_t q = t % 1600;
        if (q < 1200) sweat(c, 8, 6 + 16 * ease_in(q / 1200.0f));
    }
}

static const Anim A_TIRED = {"tired", a_tired, 6000, LOOP};
static const Anim A_LIMIT = {"limit_hit", a_limit_hit, 8000, LOOP};
static const Anim A_USAGE = {"usage", a_usage, 6000, LOOP | SCENE};
static const Anim A_CONTEXT = {"context", a_context, 5400, LOOP | SCENE};

ANIM_LIST(anims_usage, &A_TIRED, &A_LIMIT, &A_USAGE, &A_CONTEXT);
