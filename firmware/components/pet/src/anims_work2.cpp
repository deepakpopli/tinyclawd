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

static void eyes_at(Canvas& c, float lx, float rx, float y, const Eye& e) {
    Eye r = e;
    r.mirror = true;
    draw_eye(c, lx, y, e);
    draw_eye(c, rx, y, r);
}

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

// ----------------------------------------------------------------- page flip
// an open book: eyes read the left page, then the right, then the page turns

static const uint32_t PF_LINE = 360, PF_LINES = 4, PF_PAUSE = 200, PF_FLIP = 520;
static const uint32_t PF_SPREAD = 2 * PF_LINES * PF_LINE + PF_PAUSE + PF_FLIP;   // 3600
static const int PF_TOP = 38, PF_BOT = 62, PF_LX = 23, PF_RX = 105;

// words of one line on one page side: dashes with small gaps.
// left of `done` it is read and drawn dotted. sx squeezes it for the turning page
static void page_line(Canvas& c, float x0, int y, uint32_t key, int done, float sx = 1) {
    uint32_t h = hash32(key);
    int len = 18 + h % 16;
    int g1 = 4 + (h >> 5) % 8, g2 = g1 + 4 + (h >> 9) % 8;
    for (int i = 0; i < len; i++) {
        if (i == g1 || i == g2) continue;
        int xx = iround(x0 + i * sx);
        if (xx >= done || (xx + y) % 2 == 0) c.px(xx, y);
    }
}

static void page(Canvas& c, int x0, uint32_t spread, int side, uint32_t seed, int lines_read, int read_x) {
    for (int l = 0; l < (int)PF_LINES; l++) {
        int y = PF_TOP + 5 + l * 5;
        int done = l < lines_read ? 999 : (l == lines_read ? read_x : 0);
        page_line(c, x0 + 4, y, spread * 131 + side * 17 + l + seed, done);
    }
}

static void a_page_flip(Canvas& c, uint32_t t, const Ctx& x) {
    uint32_t sp = t / PF_SPREAD, in = t % PF_SPREAD;
    uint32_t read_end = 2 * PF_LINES * PF_LINE;
    // book: cover, two pages, spine
    c.hline(PF_LX - 2, PF_RX + 2, 63);
    c.vline(PF_LX - 2, PF_TOP + 2, 63);
    c.vline(PF_RX + 2, PF_TOP + 2, 63);
    c.rect(PF_LX, PF_TOP, 63, PF_BOT);
    c.rect(65, PF_TOP, PF_RX, PF_BOT);
    c.vline(64, PF_TOP + 1, 63);

    int line = in < read_end ? in / PF_LINE : 2 * PF_LINES;
    float f = in < read_end ? (in % PF_LINE) / (float)PF_LINE : 1;
    int side = line / PF_LINES, l = line % PF_LINES;
    int lx0 = (side ? 65 : PF_LX) + 4;
    float rx = lx0 + 30 * f;
    float ry = PF_TOP + 5 + l * 5;

    // left page always shows this spread, right page shows the next one under a turning page
    bool flipping = in >= read_end + PF_PAUSE;
    page(c, PF_LX, sp, 0, x.seed, side ? 99 : l, iround(rx));
    page(c, 65, flipping ? sp + 1 : sp, 1, x.seed, flipping ? 0 : (side ? l : -1), flipping ? 0 : iround(rx));

    Eye e;
    e.w = 9;
    e.h = 12;
    if (flipping) {
        float k = ease(seg(in, read_end + PF_PAUSE, PF_SPREAD));
        float tip = 64 + (PF_RX - 64) * cosf(PI * k);
        float lift = 6 * sinf(PI * k);
        float poly[] = {64, (float)PF_TOP, tip, PF_TOP - lift, tip, PF_BOT - lift, 64, (float)PF_BOT};
        c.fill_poly(poly, 4, false);
        float edge[] = {64, (float)PF_TOP, tip, PF_TOP - lift, tip, PF_BOT - lift, 64, (float)PF_BOT};
        c.polyline(edge, 4);
        // front of the turning page is the one just read, its back is the next left page
        float sx = fabsf(tip - 64) / (PF_RX - 64);
        for (int l = 0; l < (int)PF_LINES; l++) {
            int y = iround(PF_TOP + 5 + l * 5 - lift * 0.6f);
            if (k < 0.5f) page_line(c, 64 + 4 * sx, y, sp * 131 + 17 + l + x.seed, 999, sx);
            else page_line(c, tip + 4 * sx, y, (sp + 1) * 131 + l + x.seed, 0, sx);
        }
        rx = tip;
        ry = PF_TOP + 4 - lift;
        if (k > 0.5f) c.vline(64, PF_TOP, PF_BOT);
    } else if (in >= read_end) {
        rx = 101;   // end of the right page, a breath before turning
        ry = PF_BOT - 4;
    }
    e.dx = (rx - 64) / 3.5f;
    e.dy = -15 + (ry - PF_TOP) / 6;
    uint32_t h = hash32(sp * 53 + x.seed);
    if (h % 3 == 0) e = maybe_blink(e, blink_k(in, read_end + 20));
    else if (h % 3 == 1) e = maybe_blink(e, blink_k(in, PF_LINES * PF_LINE - 120));
    eyes_at(c, LEFT_X + 6, RIGHT_X - 6, EYE_Y, e);
}

// -------------------------------------------------------------- build blocks
// code blocks drop in and stack up, three rows, then the stack clears

static const uint32_t BB_ROW = 3000, BB_ROWS = 3, BB_CLEAR = 1300;
static const uint32_t BB_CYC = BB_ROW * BB_ROWS + BB_CLEAR;   // 10300

struct Row {
    int n;
    int x[9], w[9];   // blocks left to right
    int order[9];     // drop order
};

static Row block_row(uint32_t key) {
    Row r;
    r.n = 0;
    int xx = 0;
    uint32_t h = hash32(key);
    while (xx < 128 && r.n < 9) {
        int w = 12 + 4 * (hash32(h + r.n * 7) % 5);
        if (128 - xx - w < 12 || r.n == 8) w = 128 - xx;
        r.x[r.n] = xx;
        r.w[r.n] = w;
        r.n++;
        xx += w;
    }
    for (int i = 0; i < r.n; i++) r.order[i] = i;
    for (int i = r.n - 1; i > 0; i--) {
        int j = hash32(h + i * 31) % (i + 1);
        int tmp = r.order[i];
        r.order[i] = r.order[j];
        r.order[j] = tmp;
    }
    return r;
}

static void block(Canvas& c, int x0, int y0, int w, uint32_t key) {
    c.fill_rect(x0, y0 - 1, x0 + w - 1, y0 + 6, false);   // a gap around it
    c.fill_rect(x0 + 1, y0, x0 + w - 2, y0 + 5);
    // a short line of "code" cut out of it
    int indent = 2 + 2 * (hash32(key) % 3);
    int len = w - 4 - indent - (int)(hash32(key + 1) % 5);
    if (len > 1) c.hline(x0 + 1 + indent, x0 + indent + len, y0 + 2, false);
}

// where block k of a row is at time rin (top y), it falls from above the screen
static int block_y(uint32_t rin, uint32_t at, uint32_t per, int y) {
    uint32_t fall = per - 90;
    int by = iround(lerp(-8, y, ease_in(seg(rin, at, at + fall))));
    if (rin >= at + fall && rin < at + per) by += 1;   // lands with a bump
    return by;
}

static void a_build_blocks(Canvas& c, uint32_t t, const Ctx& x) {
    uint32_t cyc = t / BB_CYC, in = t % BB_CYC;
    uint32_t seed = cyc * 7001 + x.seed;
    bool clearing = in >= BB_ROW * BB_ROWS;
    uint32_t q = clearing ? in - BB_ROW * BB_ROWS : 0;
    int rows_done = clearing ? BB_ROWS : in / BB_ROW;

    // the eyes follow the block that is falling right now
    Eye e;
    e.w = 11;
    e.h = 16;
    e.dy = -10;
    if (!clearing) {
        Row r = block_row(seed + rows_done * 13);
        uint32_t rin = in % BB_ROW, per = BB_ROW / r.n;
        int k = rin / per;
        if (k >= r.n) k = r.n - 1;
        int b = r.order[k];
        float fx = r.x[b] + r.w[b] / 2.0f;
        if (k > 0) {   // glide over from the last block, no snapping
            int pb = r.order[k - 1];
            fx = lerp(r.x[pb] + r.w[pb] / 2.0f, fx, ease(seg(rin - k * per, 0, 160)));
        }
        float fy = block_y(rin, k * per, per, 57 - rows_done * 7);
        e.dx = clampf((fx - 64) / 4, -12, 12);
        e.dy = clampf(-15 + fy / 6, -15, -6);
        if (hash32(seed + k) % 5 == 0) e = maybe_blink(e, blink_k(rin - k * per, per - 60));
    } else if (q < 1100) {
        e.shape = HAPPY;
        e.w = 9;
        e.dy = -12 - 2 * sinf(PI * seg(q, 0, 500));
        if (q > 150) {
            sparkle(c, 14, 10, 3);
            sparkle(c, 113, 12, 3);
        }
    }
    face(c, e);

    // the stack. when it is done the rows flash twice, then drop away
    int shift = clearing ? iround(30 * ease_in(seg(q, 700, 1100))) : 0;
    for (int rr = 0; rr <= rows_done && rr < (int)BB_ROWS; rr++) {
        Row r = block_row(seed + rr * 13);
        int y = 57 - rr * 7 + shift;
        uint32_t rin = rr < rows_done ? BB_ROW : in % BB_ROW;
        uint32_t per = BB_ROW / r.n;
        for (int k = 0; k < r.n; k++) {
            if (rin < k * per) break;
            int b = r.order[k];
            block(c, r.x[b], block_y(rin, k * per, per, y), r.w[b], seed + rr * 100 + b);
        }
    }
    if (clearing && q < 600 && (q / 150) % 2 == 0) c.fill_rect(0, 43, 127, 63, false);
}

static const Anim A_PONDER = {"ponder", a_ponder, PD_CYC, LOOP};
static const Anim A_COMPILE = {"compile", a_compile, CP_CYC, LOOP};
static const Anim A_PAGE_FLIP = {"page_flip", a_page_flip, 3 * PF_SPREAD, LOOP};
static const Anim A_BUILD = {"build_blocks", a_build_blocks, BB_CYC, LOOP};

ANIM_LIST(anims_work2, &A_PONDER, &A_COMPILE, &A_PAGE_FLIP, &A_BUILD);
