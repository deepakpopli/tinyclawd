// more looks for while Claude Code works, so long sessions do not repeat.
// kept cheap on purpose: rects, lines, tiny circles, a few sinf per frame.
#include <stdio.h>

#include "anim.h"

static const float PI = (float)M_PI;

void spinner(Canvas& c, int x, int y, uint32_t t);   // anims_claude.cpp

// ------------------------------------------------------------------- helpers

static float blink_k(uint32_t t, uint32_t at) {
    return t >= at && t < at + BLINK_MS ? (t - at) / (float)BLINK_MS : -1;
}

static Eye maybe_blink(const Eye& e, float k) { return k >= 0 ? blink_eye(e, k) : e; }

// -------------------------------------------------------------------- ponder
// eyes pace while a thought bubble fills up with little shapes, then pops

static const uint32_t PD_ITEM = 850, PD_ITEMS = 5, PD_FULL = 1500;
static const uint32_t PD_CYC = 300 + PD_ITEMS * PD_ITEM + PD_FULL;   // 6050

static void thought_item(Canvas& c, int kind, int x, int y, bool on) {
    switch (kind) {
    case 0: c.fill_rect(x - 2, y - 2, x + 2, y + 2, on); break;
    case 1: c.rect(x - 2, y - 2, x + 2, y + 2, on); break;
    case 2: c.line(x - 2, y + 2, x, y - 2, 1, on); c.line(x, y - 2, x + 2, y + 2, 1, on); c.hline(x - 2, x + 2, y + 2, on); break;
    case 3: c.hline(x - 2, x + 2, y, on); c.vline(x, y - 2, y + 2, on); break;
    default: c.line(x - 2, y - 2, x + 2, y + 2, 1, on); c.line(x - 2, y + 2, x + 2, y - 2, 1, on); break;
    }
}

// a thought cloud: a ring of small bumps around a box
static void cloud(Canvas& c, bool lit) {
    static const int BX[] = {44, 53, 62, 71, 80, 44, 53, 62, 71, 80, 40, 84};
    static const int BY[] = {5, 4, 5, 4, 5, 13, 14, 13, 14, 13, 9, 9};
    for (int i = 0; i < 12; i++) c.fill_circle(BX[i], BY[i], 4);
    for (int i = 0; i < 12; i++) c.fill_circle(BX[i], BY[i], 3, lit);
    c.fill_rect(41, 4, 83, 14, lit);
}

static void a_ponder(Canvas& c, uint32_t t, const Ctx& x) {
    uint32_t cyc = t / PD_CYC, in = t % PD_CYC;
    uint32_t h = hash32(cyc * 977 + x.seed);
    // pacing: two steps across and back per cycle, with a little step bob
    float ph = 2 * PI * (t % (PD_CYC / 2)) / (PD_CYC / 2) + (x.seed & 1) * PI;
    Eye e;
    e.w = 11;
    e.h = 18;
    e.dx = 10 * sinf(ph);
    e.dy = 6 - 1.5f * fabsf(sinf(ph * 3));
    uint32_t items_end = 300 + PD_ITEMS * PD_ITEM;
    bool full = in >= items_end;
    uint32_t q = full ? in - items_end : 0;

    // a glance up when an item shows up (not every time)
    if (!full && in >= 300) {
        uint32_t i = (in - 300) / PD_ITEM, f = (in - 300) % PD_ITEM;
        if (hash32(h + i) % 3 != 0 && f < 420) {
            float u = sinf(PI * f / 420.0f);
            e.dy = lerp(e.dy, -2, u);
            e.dx = lerp(e.dx, -10 + (int)i * 5.0f, u);
        }
        if (hash32(h + i * 5) % 4 == 0) e = maybe_blink(e, blink_k(f, 520));
    }
    if (full) {
        e.dx = lerp(e.dx, 0, clamp01(q / 120.0f));
        if (q < 650) {   // got it
            float u = ease_out(seg(q, 0, 160));
            e.w = lerp(11, 14, u);
            e.h = lerp(18, 24, u);
            e.dy = lerp(e.dy, 2, u);
        } else if (q < 1300) {
            e.shape = HAPPY;
            e.w = 9;
            e.dy = 2 - 2 * sinf(PI * seg(q, 650, 1300));
        } else {
            e.dx = 0;
        }
    }
    face(c, e);

    // the bubble with its trail of dots towards the eyes
    bool pop = full && q >= 650;
    float ex = 64 + e.dx;
    if (!pop) {
        int a = iround(ex * 0.6f + 64 * 0.4f), b = iround(ex * 0.3f + 64 * 0.7f);
        if (in > 0) c.fill_rect(a - 1, 23, a, 24);
        if (in > 120) c.fill_rect(b - 1, 19, b + 1, 21);
    }
    if (!pop && in > 200) {
        bool lit = full && (q / 110) % 2 == 0;
        cloud(c, lit);
        int shown = in < 300 ? 0 : (in - 300) / PD_ITEM + 1;
        if (shown > (int)PD_ITEMS) shown = PD_ITEMS;
        for (int i = 0; i < shown; i++) {
            int kind = hash32(h * 3 + i) % 5;
            int ix = 46 + i * 9, iy = 9;
            uint32_t age = full ? 999 : (in - 300) - i * PD_ITEM;
            if (age < 90) iy += 2;   // new ones pop in from a bit lower
            thought_item(c, kind, ix, iy, !lit);
        }
    }
    if (pop && q < 950) {   // the pop: a burst of sparkles where the bubble was
        float k = seg(q, 650, 950);
        int r = iround(3 * (1 - k)) + 1;
        sparkle(c, 64, 9, r + 1);
        sparkle(c, iround(64 - 14 - 12 * k), iround(8 - 4 * k), r);
        sparkle(c, iround(64 + 14 + 12 * k), iround(8 - 4 * k), r);
    }
}

// ------------------------------------------------------------------- compile
// a build: progress bar with stripes, percent and the spinner, then a tick

static const uint32_t CP_RUN = 4200, CP_DONE = 1300, CP_RESET = 500;
static const uint32_t CP_CYC = CP_RUN + CP_DONE + CP_RESET;   // 6000
static const char* const FILES[] = {"main.c", "face.c", "clawd.c", "anims.c", "brain.c",
                                    "fonts.c", "canvas.c", "menu.c", "proto.c", "games.c"};

static void a_compile(Canvas& c, uint32_t t, const Ctx& x) {
    uint32_t cyc = t / CP_CYC, in = t % CP_CYC;
    uint32_t h = hash32(cyc * 1237 + x.seed);
    // uneven progress with one stall somewhere
    float a = 10 + (h & 15), b = a + 15 + ((h >> 4) & 15), d = b + 12 + ((h >> 8) & 15);
    int stall = 1 + (h >> 12) % 3;
    Key k[] = {{0, 0}, {700, a}, {1500, b}, {2300, b}, {3100, d}, {CP_RUN, 100}};
    if (stall == 1) k[2].v = a;
    if (stall == 3) k[3].v = d, k[4].v = d;
    bool stalled = false;
    for (int i = 1; i < 6; i++)
        if (in >= k[i - 1].t && in < k[i].t && k[i - 1].v == k[i].v) stalled = true;
    float p = in < CP_RUN ? KEYS(k, in) : 100;
    if (in >= CP_RUN + CP_DONE) p = 100 * (1 - ease(seg(in, CP_RUN + CP_DONE, CP_CYC)));
    bool done = in >= CP_RUN && in < CP_RUN + CP_DONE;

    const int X0 = 6, X1 = 98, Y0 = 50, Y1 = 58;
    c.rect(X0, Y0, X1, Y1);
    int head = X0 + 2 + iround((X1 - X0 - 4) * p / 100);
    uint32_t mv = done ? 0 : t / 70;
    for (int xx = X0 + 2; xx < head; xx++)
        for (int yy = Y0 + 2; yy <= Y1 - 2; yy++)
            if ((xx + yy + mv) % 6 < 4 || done) c.px(xx, yy);
    char s[8];
    snprintf(s, sizeof(s), "%d%%", iround(p));
    c.text(102, Y0 + 1, s, font_5x7);

    if (done) {
        c.text(X0, 41, "build ok", font_4x6);
        float tick[] = {108, 9, 112, 13, 121, 3};
        c.polyline(tick, 3, 2);
    } else if (in < CP_RUN) {
        int f = (int)(p / 100 * 9.99f);
        char line[16];
        snprintf(line, sizeof(line), "cc %s", FILES[(hash32(h + f) >> 3) % 10]);
        c.text(X0, 41, line, font_4x6);
        spinner(c, 108, 2, t);
    }

    Eye e;
    e.w = 11;
    e.h = 16;
    e.dy = -8;
    e.dx = clampf((head - 64) / 5.0f, -9, 9);
    if (done) {
        e.shape = HAPPY;
        e.w = 9;
        e.dy = -9 - 2 * sinf(PI * seg(in, CP_RUN, CP_RUN + 600));
    } else if (stalled) {
        e.lid = 0.35f;   // come on...
        e.dy = -6;
    }
    if (!done) e = maybe_blink(e, blink_k(in, 900 + (h >> 16) % 2000));
    face(c, e);
}

static const Anim A_PONDER = {"ponder", a_ponder, PD_CYC, LOOP};
static const Anim A_COMPILE = {"compile", a_compile, CP_CYC, LOOP};

ANIM_LIST(anims_work2, &A_PONDER, &A_COMPILE);
