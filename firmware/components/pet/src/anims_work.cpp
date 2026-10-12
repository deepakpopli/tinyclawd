// what Clawd does while Claude Code works: one look per tool, plus done / needs you
#include <stdio.h>

#include "anim.h"

static const float PI = (float)M_PI;

// ------------------------------------------------------------------- helpers

// two eyes at any spot, right one mirrored
static void eyes_at(Canvas& c, float lx, float rx, float y, const Eye& e) {
    Eye r = e;
    r.mirror = true;
    draw_eye(c, lx, y, e);
    draw_eye(c, rx, y, r);
}

// blink k for a blink starting at `at`, or -1
static float blink_k(uint32_t t, uint32_t at) {
    return t >= at && t < at + BLINK_MS ? (t - at) / (float)BLINK_MS : -1;
}

static Eye maybe_blink(const Eye& e, float k) { return k >= 0 ? blink_eye(e, k) : e; }

// --------------------------------------------------------------------- working
// Clawd at a little anvil, hammering a lump flat. sparks on every hit,
// when it is done it becomes a neat tile and slides away, next lump drops in

static const uint32_t WK_IN = 500, WK_HIT = 520, WK_HITS = 5, WK_YAY = 1100, WK_OUT = 600;
static const uint32_t WK_CYC = WK_IN + WK_HITS * WK_HIT + WK_YAY + WK_OUT;   // 4800
static const float WK_PX = 50, WK_PY = 42, WK_LEN = 24, WK_UP = -2.3f;
static const int WK_X = 74, WK_TOP = 53;   // piece centre, anvil top
static const char* const WK_MARKS[] = {"{}", "</>", "fn", "ok", "#!", "=>"};

// the piece after k hits: starts tall and narrow, ends a flat tile
static void wk_piece(Canvas& c, int cx, int bot, int k, bool done, const char* mark) {
    int w = 9 + 3 * k, h = 12 - k;
    int x0 = cx - w / 2, y0 = bot - h + 1;
    c.fill_rect(x0, y0, x0 + w - 1, bot);
    if (!done) {   // still lumpy: chipped corners and a dent on top
        c.px(x0, y0, false);
        c.px(x0 + w - 1, y0, false);
        c.px(x0 + w - 1, y0 + 1, false);
        if (k < 3) c.hline(x0 + 3, x0 + 4, y0, false);
    } else {
        c.text_center(cx, y0 + 1, mark, font_4x6, false);
    }
}

// hammer from the pivot at angle a (radians, 0 = pointing right, + = down)
static void wk_hammer(Canvas& c, float a) {
    float ca = cosf(a), sa = sinf(a);
    float tx = WK_PX + WK_LEN * ca, ty = WK_PY + WK_LEN * sa;
    c.line(WK_PX, WK_PY, tx, ty, 2);
    float nx = -sa, ny = ca;   // the way the head swings
    float hx = tx - 3 * ca, hy = ty - 3 * sa;
    float head[] = {hx - 4 * nx, hy - 4 * ny, hx - 4 * nx + 6 * ca, hy - 4 * ny + 6 * sa,
                    hx + 5 * nx + 6 * ca, hy + 5 * ny + 6 * sa, hx + 5 * nx, hy + 5 * ny};
    c.fill_poly(head, 4);
}

// angle where the bottom of the head just meets a piece with this top
static float wk_hit_angle(int top) { return (top - 6 - WK_PY) / WK_LEN; }

static void wk_anvil(Canvas& c) {
    c.fill_rect(58, WK_TOP + 1, 92, WK_TOP + 3);
    float horn[] = {92, (float)WK_TOP + 1, 103, (float)WK_TOP + 1, 92, (float)WK_TOP + 5};
    c.fill_poly(horn, 3);
    c.fill_rect(65, WK_TOP + 4, 84, WK_TOP + 6);
    c.fill_rect(69, WK_TOP + 6, 80, 60);
    c.fill_rect(61, 61, 88, 63);
}

static void a_working(Canvas& c, uint32_t t, const Ctx& x) {
    uint32_t cyc = t / WK_CYC, in = t % WK_CYC;
    uint32_t h = hash32(cyc * 613 + x.seed);
    const char* mark = WK_MARKS[h % 6];
    uint32_t hits_at = WK_IN, yay_at = WK_IN + WK_HITS * WK_HIT, out_at = yay_at + WK_YAY;

    int k = in < hits_at ? 0 : (in < yay_at ? (in - hits_at) / WK_HIT : WK_HITS);
    uint32_t u = in < hits_at ? 0 : (in - hits_at) % WK_HIT;
    // this hit lands at u = 320, the piece flattens then
    int flat = in >= yay_at ? WK_HITS : k + (u >= 320 ? 1 : 0);
    bool done = in >= yay_at;

    float a = WK_UP;
    float lie = wk_hit_angle(WK_TOP - (12 - (int)WK_HITS));
    if (in < hits_at) a = lerp(-1.5f, WK_UP, ease(seg(in, 0, 400)));
    else if (!done) {
        float prev = k == 0 ? WK_UP : wk_hit_angle(WK_TOP - (12 - k));
        float hit = wk_hit_angle(WK_TOP - (12 - k));
        if (u < 180) a = lerp(prev, WK_UP, ease(seg(u, 0, 180)));
        else if (u < 240) a = WK_UP;
        else if (u < 320) a = lerp(WK_UP, hit, ease_in(seg(u, 240, 320)));
        else a = hit - (u < 380 ? 0.08f * seg(u, 320, 380) : 0.08f * (1 - seg(u, 380, 440)));
    } else if (in < out_at) a = lerp(lie, -1.5f, ease_out(seg(in, yay_at, yay_at + 250)));
    else a = -1.5f;

    wk_anvil(c);
    c.hline(0, 127, 63);
    // a lamp over the anvil that swings a little after each hit
    uint32_t since = in >= hits_at && !done ? (u >= 320 ? u - 320 : u + WK_HIT - 320) : 999;
    float sw = since < 500 ? 3 * (1 - since / 500.0f) * sinf(since / 40.0f) : 0;
    const int LAMP = 104;
    float lx = LAMP + sw;
    c.line(LAMP, 0, lx, 7);
    float shade[] = {lx - 3, 7, lx + 3, 7, lx + 7, 12, lx - 7, 12};
    c.fill_poly(shade, 4);
    c.fill_rect(iround(lx) - 1, 13, iround(lx) + 1, 14);
    for (int i = -1; i <= 1; i++) c.px(iround(lx + i * 9), 17 + (i ? 0 : 2));

    // the piece: drops in, gets hit, slides off to the right when done
    int px = WK_X, bot = WK_TOP;
    if (in < hits_at) bot = iround(lerp(-14, WK_TOP, ease_in(seg(in, 0, 320))));
    if (in >= out_at) px = iround(lerp(WK_X, 150, ease_in(seg(in, out_at, WK_CYC))));
    bool squash = !done && u >= 320 && u < 380;   // squished a bit more right on impact
    wk_piece(c, px, bot, flat + (squash ? 1 : 0), done, mark);
    if (in >= hits_at && in < hits_at + 60) c.hline(WK_X - 7, WK_X + 7, WK_TOP + 1, false);   // landing thud

    // sparks fly from the impact
    if (!done && u >= 320 && u < 760 && in >= hits_at) {
        float s = (u - 320) / 1000.0f;
        int top = WK_TOP - (12 - flat);
        if (u < 400) {   // a quick flash right where it hit
            c.line(WK_X - 9, top - 2, WK_X - 13, top - 5);
            c.line(WK_X + 9, top - 2, WK_X + 13, top - 5);
            c.line(WK_X - 9, top + 3, WK_X - 13, top + 3);
            c.line(WK_X + 9, top + 3, WK_X + 13, top + 3);
        }
        for (int i = 0; i < 8; i++) {
            uint32_t g = hash32(h + k * 31 + i * 7);
            if (s * 1000 > 260 + g % 180) continue;   // each one burns out on its own
            float vx = (int)(g % 180) - 90;
            float vy = -90 - (float)((g >> 8) % 90);
            float sx = WK_X + vx * s, sy = top + vy * s + 400 * s * s;
            c.line(sx, sy, sx - vx * 0.025f, sy - (vy + 800 * s) * 0.025f);
        }
    }
    if (done && in < out_at) {   // shiny new tile
        bool alt = ((in - yay_at) / 160) % 2;
        sparkle(c, WK_X - 16, alt ? 40 : 38, alt ? 2 : 3);
        sparkle(c, WK_X + 15, alt ? 36 : 39, alt ? 3 : 2);
    }

    // Clawd: watches, winces on the hit, hops when the tile is done
    Pose p = pose(EYES_RIGHT, ARMS_ONE_UP);
    int cy = 39;
    if (!done && in >= hits_at && u >= 320 && u < 420) {
        p.eyes = EYES_CLOSED;
        cy += 1;
    }
    if (done && in < out_at) {
        uint32_t q = in - yay_at;
        p.eyes = q < 700 ? EYES_CLOSED : EYES_RIGHT;
        if (q > 100 && q < 700) cy -= iround(5 * fabsf(sinf(2 * PI * seg(q, 100, 700))));
    }
    if (in >= out_at) p.eyes = EYES_RIGHT;
    if ((h >> 9) % 3 == 0 && !done && k >= 3) sweat(c, 10, 30 + 6 * seg(in, yay_at - 2 * WK_HIT, yay_at));
    clawd(c, 14, cy, p);
    wk_hammer(c, a + (cy - 39) * 0.02f);
}

// ------------------------------------------------------------------- searching
// a magnifier goes through the code line by line, matches light up,
// and a little hourglass runs in the corner

static const int SR_LINES = 5, SR_X0 = 4, SR_X1 = 94;
static const uint32_t SR_LINE = 720, SR_HOLD = 900;
static const uint32_t SR_CYCLE = SR_LINES * SR_LINE + SR_HOLD;

struct CodeLine {
    int n;          // words
    int x[4], w[4];
    int match;      // word index that matches, or -1
};

static CodeLine code_line_at(uint32_t cycle, int i, uint32_t seed) {
    uint32_t h = hash32(cycle * 97 + i * 13 + seed);
    CodeLine l;
    int xx = SR_X0 + (i == 0 ? 0 : (h % 3) * 8);
    l.n = 2 + (h >> 4) % 3;
    for (int k = 0; k < l.n; k++) {
        l.x[k] = xx;
        l.w[k] = 6 + ((h >> (8 + k * 4)) & 7) * 3;
        if (xx + l.w[k] > SR_X1) l.w[k] = SR_X1 - xx;
        xx += l.w[k] + 5;
        if (xx > SR_X1 - 6) { l.n = k + 1; break; }
    }
    l.match = (h >> 24) % 5 < 2 || i == (int)(cycle + seed) % SR_LINES ? (h >> 28) % l.n : -1;
    return l;
}

// sand f = 0..1 already fallen, k = 1 upright .. -1 upside down (for the flip)
static void hourglass(Canvas& c, float cx, float cy, float f, float k) {
    const float H = 14, W = 9;
    c.line(cx - W, cy - H * k, cx + W, cy - H * k, 2);
    c.line(cx - W, cy + H * k, cx + W, cy + H * k, 2);
    float glass[] = {cx - W + 2, cy - (H - 1) * k, cx - 1, cy, cx - W + 2, cy + (H - 1) * k};
    float glass2[] = {cx + W - 2, cy - (H - 1) * k, cx + 1, cy, cx + W - 2, cy + (H - 1) * k};
    c.polyline(glass, 3);
    c.polyline(glass2, 3);
    // top sand sits in the neck and drains, the pile grows at the bottom
    float top = (H - 2) * (1 - f), pile = (H - 5) * f;
    if (top > 0.5f) {
        float hw = (W - 2) * top / (H - 1) - 1;
        float p[] = {cx - hw, cy - top * k, cx + hw, cy - top * k, cx, cy - 1 * k};
        c.fill_poly(p, 3);
    }
    if (pile > 0.5f) {
        float base = cy + (H - 2) * k;
        float p[] = {cx - W + 3, base, cx + W - 3, base, cx, base - pile * k};
        c.fill_poly(p, 3);
    }
    if (f < 1 && k > 0.9f) c.vline(iround(cx), iround(cy), iround(cy + H - 2 - pile));
}

static void a_searching(Canvas& c, uint32_t t, const Ctx& x) {
    uint32_t cycle = t / SR_CYCLE, in = t % SR_CYCLE;
    int cur = in / SR_LINE;   // line being searched, SR_LINES = all done
    float f = (in % SR_LINE) / (float)SR_LINE;
    float lens_x = lerp(SR_X0 + 4, SR_X1 - 4, ease(f));

    int hits = 0;
    bool just_found = false;
    for (int i = 0; i < SR_LINES; i++) {
        CodeLine l = code_line_at(cycle, i, x.seed);
        int y = 15 + i * 10;
        for (int k = 0; k < l.n; k++) {
            bool lit = k == l.match && (i < cur || (i == cur && lens_x > l.x[k] + l.w[k] / 2));
            if (lit) {
                hits++;
                if (i == cur && lens_x < l.x[k] + l.w[k] / 2 + 14) just_found = true;
                c.fill_rect(l.x[k] - 2, y - 3, l.x[k] + l.w[k] + 1, y + 4);
                c.fill_rect(l.x[k], y - 1, l.x[k] + l.w[k] - 1, y + 1, false);
            } else {
                c.fill_rect(l.x[k], y - 1, l.x[k] + l.w[k] - 1, y + 1);
            }
        }
    }

    // header: what is searching and how many it found
    c.fill_rect(0, 0, 127, 9);
    const char* what = !strcmp(x.tool, "Glob") ? "search glob" : "search grep";
    c.text(3, 1, what, font_5x7, false);
    char buf[12];
    snprintf(buf, sizeof(buf), "%d", hits);
    c.text(124 - c.text_width(buf), 1, buf, font_5x7, false);

    // magnifier on the line being searched, it hops a bit when it finds one
    if (cur < SR_LINES) {
        float ly = 15 + cur * 10 - (just_found ? 2 : 0);
        c.fill_circle(lens_x, ly, 6);
        c.fill_circle(lens_x, ly, 4, false);
        c.line(lens_x + 4, ly + 4, lens_x + 9, ly + 9, 3);
    }

    // hourglass: sand runs for a whole pass, then it flips
    float sand = clamp01(in / (float)(SR_CYCLE - 400));
    float k = 1;
    if (in >= SR_CYCLE - 400) {
        k = cosf(PI * seg(in, SR_CYCLE - 400, SR_CYCLE));
        if (k < 0) sand = 0;   // past the half: the full end is now on top
    }
    hourglass(c, 115, 36, sand, k < 0 ? -k : k);
}

// ------------------------------------------------------------------- needs you

static const float NY[] = {0, -3, -4, -3, 0, 0, 0, 0};

static void a_needs_you(Canvas& c, uint32_t t, const Ctx& x) {
    Eye e;
    e.w = 15;
    e.h = 26;
    e.dy = pickf(NY, 8, t / 90.0f) + 3;
    // looks around for you now and then
    uint32_t slot = t / 2400, in = t % 2400;
    uint32_t h = hash32(slot * 41 + x.seed);
    if ((h & 3) == 0 && in > 900 && in < 1900) {
        float u = sinf(PI * seg(in, 900, 1900));
        e.dx = ((h >> 4) & 1 ? 8 : -8) * u;
    }
    face(c, maybe_blink(e, (h & 3) == 1 ? blink_k(in, 1300) : -1));
    // big ! on both sides, blinking
    if ((t / 360) % 2 == 0) {
        c.text(6, 20, "!", font_bold24);
        c.text(115, 20, "!", font_bold24);
    }
    if (*x.who) c.text_center(64, 57, x.who, font_5x7);   // which project
    // a hard flash every few seconds so it is seen from across the room
    uint32_t f = t % 2880;
    if (f < 90 || (f >= 180 && f < 270)) c.invert();
}

// ------------------------------------------------------------------------ done

static const float HOP[] = {0, -4, -6, -4, 0, 0, 0};

static void sparkles4(Canvas& c, bool alt) {
    static const int A[][2] = {{14, 10}, {112, 14}, {20, 50}, {108, 48}};
    static const int B[][2] = {{10, 20}, {116, 34}, {26, 56}, {102, 56}};
    for (int k = 0; k < 4; k++) sparkle(c, alt ? B[k][0] : A[k][0], alt ? B[k][1] : A[k][1]);
}

static void a_done(Canvas& c, uint32_t t, const Ctx&) {
    if (t < 300 || t >= 1780) {
        face(c);
        return;
    }
    if (t < 420) {
        face(c, blink_eye(Eye(), 0.5f * seg(t, 300, 420)));
        return;
    }
    if (t >= 1660) {
        face(c, blink_eye(Eye(), 0.5f + 0.5f * seg(t, 1660, 1780)));
        return;
    }
    Eye e;
    e.shape = HAPPY;
    e.dy = pickf(HOP, 7, (t - 420) / 90.0f);
    face(c, e);
    sparkles4(c, ((t - 420) / 180) % 2);
}

// ----------------------------------------------------------------------- party

static void confetti(Canvas& c, uint32_t t, uint32_t seed) {
    for (uint32_t k = 0; k < 50; k++) {
        uint32_t h = hash32(k * 97 + seed);
        uint32_t born = (h >> 8) % 1600;
        if (t < born) continue;
        float v = 0.05f + ((h >> 16) % 100) / 100.0f * 0.03f;
        float y = -4 + v * (t - born);
        if (y > 64) continue;
        float x0 = (h & 0x7f);
        float xx = x0 + 4 * sinf(t / 210.0f + x0);
        int kind = (h >> 24) % 3;
        if (kind == 0) c.fill_rect(iround(xx), iround(y), iround(xx) + 1, iround(y) + 1);
        else if (kind == 1) c.line(xx, y, xx + 1, y + 2);
        else c.line(xx, y + 2, xx + 2, y);
    }
}

static const float PB[] = {0, -3, -4, -3, 0, 0};

static void a_party(Canvas& c, uint32_t t, const Ctx& x) {
    if (t < 200 || t >= 3020) face(c);
    else if (t < 320) face(c, blink_eye(Eye(), 0.5f * seg(t, 200, 320)));
    else if (t >= 2900) face(c, blink_eye(Eye(), 0.5f + 0.5f * seg(t, 2900, 3020)));
    else {
        Eye e;
        e.shape = HAPPY;
        e.dy = pickf(PB, 6, (t - 320) / 70.0f);
        face(c, e);
    }
    if (t >= 200) confetti(c, t - 200, x.seed);
}

// --------------------------------------------------------------------- reading

struct Line {
    int x0, len, nw;
    uint8_t w[10];   // word widths
};

static Line text_line(uint32_t n, uint32_t seed) {
    Line l;
    uint32_t h = hash32(n * 7349 + seed);
    l.x0 = (h & 7) == 0 ? 28 : 20;
    int maxlen = (h >> 3) % 5 == 0 ? 30 + (h >> 6) % 30 : 70 + (h >> 6) % 18;
    l.len = 0;
    l.nw = 0;
    while (l.nw < 10) {
        int w = 3 + hash32(h + l.nw * 13) % 9;
        if (l.len + w > maxlen && l.nw) break;
        l.w[l.nw++] = w;
        l.len += w + 3;
    }
    l.len -= 3;
    return l;
}

// words left of `done` are already read: drawn as a dotted checker
static void draw_line(Canvas& c, const Line& l, int y, int done) {
    int xx = l.x0;
    for (int k = 0; k < l.nw; k++) {
        for (int i = xx; i < xx + l.w[k]; i++) {
            bool read = i < done;
            c.px(i, y, !read || (i + y) % 2 == 0);
            c.px(i, y + 1, !read || (i + y) % 2 == 1);
        }
        xx += l.w[k] + 3;
    }
}

static const int RD_MS_PX = 16, RD_BACK = 260, RD_PAUSE = 1000;

static uint32_t line_ms(const Line& l, uint32_t n, uint32_t seed, bool& pause) {
    pause = hash32(n * 31 + seed + 5) % 7 == 0;
    return l.len * RD_MS_PX + RD_BACK + (pause ? RD_PAUSE : 0);
}

static void a_reading(Canvas& c, uint32_t t, const Ctx& x) {
    uint32_t n = 0, start = 0;
    bool pause;
    Line cur = text_line(0, x.seed);
    uint32_t len = line_ms(cur, 0, x.seed, pause);
    while (t >= start + len) {
        start += len;
        cur = text_line(++n, x.seed);
        len = line_ms(cur, n, x.seed, pause);
    }
    uint32_t in = t - start, read = cur.len * RD_MS_PX;
    // eyes hop word by word along the line
    uint32_t px = in / RD_MS_PX, ws = 0, prev = 0;
    for (int k = 0; k < cur.nw - 1; k++) {
        uint32_t next = ws + cur.w[k] + 3;
        if (px < next) break;
        prev = ws;
        ws = next;
    }
    float rx = cur.x0 + lerp(prev, ws, ease(clamp01((px - (float)ws) / 4.0f)));
    float rx_end = cur.x0 + ws;
    float scroll = 0;   // 0..1 while the lines move up
    Eye e;
    e.w = 8;
    e.h = 10;
    e.dy = -12;
    if (in >= read) {
        uint32_t q = in - read;
        if (pause && q < RD_PAUSE) {   // stops to think, looks up at you
            float u = sinf(PI * seg(q, 0, RD_PAUSE));
            rx = rx_end;
            e.dy -= 6 * u;
            e.h = lerp(10, 14, u);
            e = maybe_blink(e, blink_k(q, 450));
        } else {
            uint32_t b = q - (pause ? RD_PAUSE : 0);
            float k = ease(seg(b, 0, RD_BACK - 60));
            rx = lerp(rx_end, text_line(n + 1, x.seed).x0, k);
            scroll = k;
        }
    }
    e.dx = (rx - 64) / 3.5f;
    if (hash32(n + x.seed * 3) % 4 == 0) e = maybe_blink(e, blink_k(in, read / 2));
    Eye l = e, r = e;
    r.mirror = true;
    draw_eye(c, LEFT_X + 6, EYE_Y, l);
    draw_eye(c, RIGHT_X - 6, EYE_Y, r);
    // text: lines already read above, the current one, the next ones below
    for (int j = -2; j <= 4; j++) {
        if ((int)n + j < 0) continue;
        int y = 40 + j * 6 - iround(scroll * 6);
        if (y < 34 || y > 62) continue;
        int done = j < 0 ? 999 : (j == 0 ? (scroll > 0 ? 999 : iround(rx) + 1) : 0);
        draw_line(c, text_line(n + j, x.seed), y, done);
    }
}

// ---------------------------------------------------------------------- typing

static const char* const NAMES[] = {"x", "i", "n", "ok", "buf", "len", "eye", "pet", "tick", "draw", "t", "acc"};
static const char* const KW[] = {"fn", "let", "int", "auto", "void"};
static const char* const OPS[] = {" = ", " += ", " < ", " == ", " - "};

static void code_line(uint32_t n, uint32_t seed, char* out, int cap) {
    uint32_t h = hash32(n * 3301 + seed);
    const char* a = NAMES[(h >> 4) % 12];
    const char* b = NAMES[(h >> 8) % 12];
    const char* op = OPS[(h >> 12) % 5];
    switch (h % 8) {
    case 0: snprintf(out, cap, "%s %s(%s) {", KW[(h >> 16) % 5], a, b); break;
    case 1: snprintf(out, cap, "  %s%s%s;", a, op, b); break;
    case 2: snprintf(out, cap, "  %s(%s);", a, b); break;
    case 3: snprintf(out, cap, "}"); break;
    case 4: snprintf(out, cap, "  if (%s%s%d) {", a, op, (int)((h >> 20) % 10)); break;
    case 5: snprintf(out, cap, "  return %s;", a); break;
    case 6: snprintf(out, cap, "// %s %s", a, b); break;
    default: snprintf(out, cap, "  %s++;", a); break;
    }
}

static const uint32_t TY_CH = 85, TY_HOLD = 450, TY_SCROLL = 140;

static uint32_t typing_ms(uint32_t n, uint32_t seed) {
    char s[32];
    code_line(n, seed, s, sizeof(s));
    return strlen(s) * TY_CH + TY_HOLD + TY_SCROLL;
}

static void a_typing(Canvas& c, uint32_t t, const Ctx& x) {
    uint32_t n = 0, start = 0, len = typing_ms(0, x.seed);
    while (t >= start + len) {
        start += len;
        len = typing_ms(++n, x.seed);
    }
    uint32_t in = t - start;
    char cur[32];
    code_line(n, x.seed, cur, sizeof(cur));
    int nch = strlen(cur);
    // a little thinking pause somewhere in the line
    uint32_t h = hash32(n * 7 + x.seed);
    int stop = (h & 1) ? 2 + (h >> 3) % (nch > 3 ? nch - 2 : 1) : 999;
    uint32_t typed_ms = in;
    if ((int)(in / TY_CH) > stop) typed_ms = in < (uint32_t)stop * TY_CH + 400 ? stop * TY_CH : in - 400;
    int shown = typed_ms / TY_CH;
    if (shown > nch) shown = nch;
    uint32_t end_type = nch * TY_CH + (stop < nch ? 400 : 0);
    float scroll = in > end_type + TY_HOLD ? ease(seg(in, end_type + TY_HOLD, len)) : 0;
    int base = 56 - iround(scroll * 8);
    char buf[32];
    for (int j = 3; j >= 1; j--) {
        if ((int)n - j < 0) continue;
        int y = base - j * 8;
        if (y < 36) continue;
        code_line(n - j, x.seed, buf, sizeof(buf));
        c.text(2, y, buf, font_5x7);
    }
    buf[0] = 0;
    strncpy(buf, cur, shown);
    buf[shown] = 0;
    int cx = c.text(2, base, buf, font_5x7);
    bool typing = in < end_type;
    if (scroll == 0 && (typing || (t / 265) % 2 == 0)) c.fill_rect(cx, base, cx + 4, base + 6);
    // eyes look down at the cursor
    Eye e;
    e.h = 16;
    e.w = 11;
    e.dy = -8;
    e.dx = (cx - 64) / 6.0f;
    if (h % 3 == 0) e = maybe_blink(e, blink_k(in, end_type + 60));
    if (typing && shown > 0 && (h >> 5) % 4 == 0) e.dy += 0.6f * ((in / 90) % 2);   // typing hard
    face(c, e);
}

// -------------------------------------------------------------------- terminal
// a full screen terminal: title bar, real commands typing out, output scrolling,
// and a big check or x when a command finishes

struct Cmd {
    const char* cmd;
    const char* out[3];
    int nout;
    bool ok;    // false = it failed, shows the x
    bool bar;   // a build: a progress bar before the output
};

static const Cmd CMDS[] = {
    {"npm test", {"> jest", "PASS 12 tests", "done in 1.4s"}, 3, true, false},
    {"ls", {"src  docs  test", "README.md"}, 2, true, false},
    {"make", {"cc main.c", "cc pet.c", "ld tinyclawd"}, 3, true, true},
    {"git status", {"on branch main", "2 files changed"}, 2, true, false},
    {"cargo build", {"Compiling pet", "Finished"}, 2, true, true},
    {"pytest -q", {"..F.....", "1 failed"}, 2, false, false},
    {"git diff", {"+ eyes.y += 1", "- eyes.y -= 1"}, 2, true, false},
    {"idf.py build", {"Linking...", "build ok"}, 2, true, true},
    {"npm run lint", {"pet.ts:12", "error: no-var"}, 2, false, false},
    {"grep -rn TODO", {"pet.c:12 TODO", "face.c:40 TODO"}, 2, true, false},
};
static const int NCMD = sizeof(CMDS) / sizeof(CMDS[0]);
static const uint32_t TM_CH = 90, TM_WAIT = 400, TM_BAR = 1400, TM_OUT = 300, TM_MARK = 700, TM_IDLE = 500;
static const int TM_ROWS = 5;

static int block_cmd(uint32_t b, uint32_t seed) {
    int k = hash32(b * 613 + seed) % NCMD;
    if (b > 0 && k == (int)(hash32((b - 1) * 613 + seed) % NCMD)) k = (k + 3) % NCMD;
    return k;
}

static uint32_t wait_ms(const Cmd& m) { return m.bar ? TM_BAR : TM_WAIT; }

static uint32_t block_ms(int k) {
    const Cmd& m = CMDS[k];
    return strlen(m.cmd) * TM_CH + wait_ms(m) + m.nout * TM_OUT + TM_MARK + TM_IDLE;
}

static void big_check(Canvas& c, int x, int y) {
    float p[] = {(float)x + 5, (float)y + 14, (float)x + 11, (float)y + 20, (float)x + 23, (float)y + 6};
    c.polyline(p, 3, 4, false);
}

static void big_x(Canvas& c, int x, int y) {
    c.line(x + 7, y + 7, x + 20, y + 20, 4, false);
    c.line(x + 7, y + 20, x + 20, y + 7, 4, false);
}

static void a_terminal(Canvas& c, uint32_t t, const Ctx& x) {
    // title bar
    c.fill_rect(0, 0, 127, 9);
    for (int k = 0; k < 3; k++) c.fill_rect(3 + k * 6, 3, 5 + k * 6, 5, false);
    c.text_center(64, 1, "bash", font_5x7, false);

    uint32_t b = 0, start = 0;
    int k = block_cmd(0, x.seed);
    while (t >= start + block_ms(k)) {
        start += block_ms(k);
        k = block_cmd(++b, x.seed);
    }
    uint32_t in = t - start;
    const Cmd& cm = CMDS[k];
    uint32_t typed = strlen(cm.cmd) * TM_CH;
    uint32_t ran = typed + wait_ms(cm);
    uint32_t done_at = ran + cm.nout * TM_OUT;

    // lines bottom up: 0 is the newest
    const char* lines[TM_ROWS];
    int chars[TM_ROWS];
    bool prompt[TM_ROWS];
    int n = 0;
    auto push = [&](const char* s, int ch, bool p) {
        if (n < TM_ROWS) { lines[n] = s; chars[n] = ch; prompt[n] = p; n++; }
    };
    int outs = in < ran ? 0 : 1 + (in - ran) / TM_OUT;
    if (outs > cm.nout) outs = cm.nout;
    bool done = in >= done_at;
    bool barring = cm.bar && in >= typed && in < ran;
    if (done) push("", 0, true);   // the next prompt
    for (int o = outs - 1; o >= 0; o--) push(cm.out[o], 99, false);
    if (barring) push("", 0, false);   // room for the bar
    push(cm.cmd, in / TM_CH, true);
    for (int bb = (int)b - 1; bb >= 0 && n < TM_ROWS; bb--) {
        const Cmd& p = CMDS[block_cmd(bb, x.seed)];
        for (int o = p.nout - 1; o >= 0; o--) push(p.out[o], 99, false);
        push(p.cmd, 99, true);
    }

    int cur_x = -1, cur_y = 0;
    for (int i = 0; i < n; i++) {
        int y = 13 + (n - 1 - i) * 10;
        int xx = 2;
        if (prompt[i]) {
            c.text(xx, y, "$", font_6x10);   // bold: drawn twice
            c.text(xx + 1, y, "$", font_6x10);
            xx += 10;
        }
        char buf[22];
        int m = chars[i], sl = strlen(lines[i]);
        if (m > sl) m = sl;
        if (m > 20) m = 20;
        memcpy(buf, lines[i], m);
        buf[m] = 0;
        int ex = c.text(xx, y, buf, font_6x10);
        if (i == 0 && prompt[i]) { cur_x = ex; cur_y = y; }
        if (barring && i == 0) {   // build progress
            float f = seg(in, typed, ran);
            c.rect(xx, y + 1, xx + 80, y + 7);
            c.fill_rect(xx + 2, y + 3, xx + 2 + iround(76 * f), y + 5);
        }
    }
    // fat block cursor: solid while typing, blinks while idle, gone while running
    bool running = !done && in >= typed;
    if (cur_x >= 0 && !running && (in < typed || (t / 400) % 2 == 0))
        c.fill_rect(cur_x + 1, cur_y, cur_x + 6, cur_y + 9);

    // the result, big, for a moment
    if (in >= done_at && in < done_at + TM_MARK) {
        int bx = 92, by = 20;
        c.fill_rect(bx - 2, by - 2, bx + 29, by + 29, false);
        c.fill_rect(bx, by, bx + 27, by + 27);
        if (cm.ok) big_check(c, bx, by);
        else big_x(c, bx, by);
    }
}

// ------------------------------------------------------------------------- web
// a globe spins on the right, he watches it from the left and data flies in

// unit circle, 16 points, so the globe needs no sin/cos per point
static const float UC[16] = {1, 0.924f, 0.707f, 0.383f, 0, -0.383f, -0.707f, -0.924f,
                             -1, -0.924f, -0.707f, -0.383f, 0, 0.383f, 0.707f, 0.924f};
static const float US[16] = {0, 0.383f, 0.707f, 0.924f, 1, 0.924f, 0.707f, 0.383f,
                             0, -0.383f, -0.707f, -0.924f, -1, -0.924f, -0.707f, -0.383f};

// an ellipse as a 16 point loop, cheap enough for every frame
static void loop16(Canvas& c, float cx, float cy, float rx, float ry, int width = 1) {
    for (int k = 0; k < 16; k++) {
        int j = (k + 1) % 16;
        c.line(cx + rx * UC[k], cy + ry * US[k], cx + rx * UC[j], cy + ry * US[j], width);
    }
}

static void globe(Canvas& c, float gx, float gy, float r, float rot) {
    loop16(c, gx, gy, r, r, 2);
    c.hline(iround(gx - r), iround(gx + r), iround(gy));
    c.hline(iround(gx - r * 0.85f), iround(gx + r * 0.85f), iround(gy - r * 0.5f));
    c.hline(iround(gx - r * 0.85f), iround(gx + r * 0.85f), iround(gy + r * 0.5f));
    // meridians turning: a meridian is half an ellipse, only the front ones show
    for (int m = 0; m < 4; m++) {
        float a = rot + m * PI / 4;
        a = fmodf(a, PI) - PI / 2;   // keep it on the front, -90..90 degrees
        float rx = r * sinf(a);
        if (fabsf(rx) < 1) {
            c.vline(iround(gx), iround(gy - r), iround(gy + r));
            continue;
        }
        for (int k = 12; k < 20; k++) {   // bottom pole round to the top pole
            int i = k % 16, j = (k + 1) % 16;
            c.line(gx + rx * UC[i], gy + r * US[i], gx + rx * UC[j], gy + r * US[j]);
        }
    }
}

static void a_web(Canvas& c, uint32_t t, const Ctx& x) {
    const float GX = 105, GY = 31, GR = 13;
    float bob = (t / 700) % 2 ? 1 : 0;
    globe(c, GX, GY + bob, GR, t / 900.0f);
    uint32_t pq = t % 3700;   // a ping now and then
    if (pq < 400) {
        float r = GR + 3 + 8 * pq / 400.0f;
        for (int k = 0; k < 16; k++) c.px(iround(GX + r * UC[k]), iround(GY + bob + r * US[k]));
    }

    // eyes on the left watching the globe, now and then a look at you
    uint32_t slot = t / 2600, in = t % 2600;
    uint32_t h = hash32(slot * 59 + x.seed);
    Eye e;
    e.w = 12;
    e.h = 19;
    e.dx = 4;
    e.dy = -1;
    if ((h & 3) == 0 && in > 600 && in < 1700) {
        float u = sinf(PI * seg(in, 600, 1700));
        e.dx = lerp(e.dx, -2, u);
        e.dy = lerp(e.dy, -3, u);
    }
    eyes_at(c, 30, 62, 32, maybe_blink(e, (h & 3) == 1 ? blink_k(in, 1100) : -1));

    // data coming in: little packets from the globe to the eyes
    for (int k = 0; k < 3; k++) {
        float p = fmodf(t / 1000.0f + k / 3.0f, 1);
        float px = lerp(GX - GR - 3, 72, p);
        float py = GY + (k - 1) * 6 * (1 - p);
        c.fill_rect(iround(px), iround(py), iround(px) + 1, iround(py) + 1);
    }
}

// ------------------------------------------------------------------------ todo

static void a_todo(Canvas& c, uint32_t t, const Ctx& x) {
    int n = 3 + (x.seed >> 3) % 2;
    const uint32_t IN = 300, T0 = 700, STEP = 500;
    uint32_t t_happy = T0 + n * STEP, t_out = t_happy + 600, T_END = t_out + 400;
    float m = t < IN ? ease(seg(t, 0, IN)) : (t >= t_out ? 1 - ease(seg(t, t_out, T_END)) : 1);
    float lx = lerp(LEFT_X, 18, m), rx = lerp(RIGHT_X, 42, m);
    int listx = iround(lerp(130, 60, m));
    int top = 32 - n * 11 / 2;
    int cur = t < T0 ? 0 : (t - T0) / STEP;   // item being ticked
    for (int i = 0; i < n; i++) {
        int y = top + i * 11;
        c.rect(listx, y, listx + 6, y + 6);
        int len = 18 + hash32(i * 11 + x.seed) % 32;
        bool ticked = t >= T0 + i * STEP + 200;
        if (ticked) {
            float pts[] = {(float)listx + 1, (float)y + 3, (float)listx + 3, (float)y + 5, (float)listx + 7, (float)y - 1};
            c.polyline(pts, 3, 2);
            for (int xx = listx + 11; xx < listx + 11 + len; xx += 2) c.px(xx, y + 3);
        } else c.fill_rect(listx + 11, y + 2, listx + 11 + len, y + 3);
    }
    Eye e;
    e.w = lerp(EYE_W, 10, m);
    e.h = lerp(EYE_H, 18, m);
    if (t >= t_happy && t < t_out) {
        e.shape = HAPPY;
        e.w = 8;
        e.dy = -2 * sinf(PI * seg(t, t_happy, t_out));
    } else if (t >= T0 && cur < n) {
        int y = top + cur * 11 + 3;
        uint32_t in = (t - T0) % STEP;
        e.dy = (y - 32) / 3.0f + 3 * sinf(PI * seg(in, 200, 380));   // nod on the tick
        e.dx = 3;
    }
    if (m > 0 && m < 1) e.dy = 0;
    eyes_at(c, lx, rx, EYE_Y, e);
}

// ------------------------------------------------------------------------ plug
// a plug goes into a battery, a bolt shows up and the bars fill: charging

struct PlugCycle {
    bool miss;      // bumps the socket first and tries again
    uint32_t len;
};

static const uint32_t PL_IN = 700, PL_MISS = 800, PL_SPARK = 300, PL_BAR = 450, PL_FULL = 1300, PL_OUT = 500;

static PlugCycle plug_cycle(uint32_t i, uint32_t seed) {
    uint32_t h = hash32(i * 1931 + seed);
    PlugCycle p;
    p.miss = i > 0 && h % 3 == 0;
    p.len = PL_IN + (p.miss ? PL_MISS : 0) + PL_SPARK + 4 * PL_BAR + PL_FULL + PL_OUT;
    return p;
}

static void bolt(Canvas& c, float x, float y, bool on) {
    float p[] = {x + 3, y - 12, x - 6, y + 2, x, y + 2, x - 3, y + 12, x + 6, y - 2, x, y - 2};
    c.fill_poly(p, 6, on);
}

static void a_plug(Canvas& c, uint32_t t, const Ctx& x) {
    uint32_t i = 0, start = 0;
    PlugCycle p = plug_cycle(0, x.seed);
    while (t >= start + p.len) {
        start += p.len;
        p = plug_cycle(++i, x.seed);
    }
    uint32_t in = t - start;
    uint32_t miss = p.miss ? PL_MISS : 0;
    uint32_t on_at = PL_IN + miss;                      // plugged in from here
    uint32_t out_at = p.len - PL_OUT;                   // pulled out from here

    // plug front x: slides in from the right, maybe misses once
    const float OUT = 124, IN = 94;
    float px = IN, py = 0;
    if (in < PL_IN) px = lerp(OUT, IN + 2, ease(seg(in, 0, PL_IN)));
    else if (in < on_at) {
        const Key mx[] = {{0, IN + 2}, {120, IN + 1}, {300, IN + 8}, {650, IN + 8}, {800, IN + 2}};
        const Key my[] = {{120, -4}, {300, -3}, {520, 0}};
        px = KEYS(mx, in - PL_IN);
        py = KEYS(my, in - PL_IN);
    } else if (in < on_at + 100) px = lerp(IN + 2, IN, seg(in, on_at, on_at + 100));
    else if (in >= out_at) px = lerp(IN, OUT + 6, ease(seg(in, out_at, p.len)));
    bool plugged = in >= on_at + 100 && in < out_at;

    // battery
    c.rect(12, 14, 86, 50);
    c.rect(13, 15, 85, 49);
    c.fill_rect(87, 25, 91, 39);
    int bars = 0;
    if (plugged && in >= on_at + PL_SPARK) bars = 1 + (in - on_at - PL_SPARK) / PL_BAR;
    if (bars > 4) bars = 4;
    if (in >= out_at) bars = 4;
    for (int k = 0; k < bars; k++) c.fill_rect(17 + k * 17, 18, 30 + k * 17, 46);
    uint32_t full_at = on_at + PL_SPARK + 3 * PL_BAR + PL_BAR;
    if (plugged && in >= full_at && in < full_at + 900) {   // full: sparkles pop at the corners
        bool alt = ((in - full_at) / 150) % 2;
        sparkle(c, alt ? 8 : 6, alt ? 8 : 56, 3);
        sparkle(c, alt ? 92 : 94, alt ? 54 : 10, 3);
    }
    if (plugged || in >= out_at) {   // the bolt, with a dark edge so it shows on the bars
        bolt(c, 48, 32, false);
        bolt(c, 50, 32, false);
        bolt(c, 49, 31, false);
        bolt(c, 49, 33, false);
        bolt(c, 49, 32, true);
    }

    // plug and cable from the right
    int y0 = 32 + iround(py);
    c.fill_rect(iround(px), y0 - 3, iround(px) + 2, y0 - 3);
    c.fill_rect(iround(px), y0 + 3, iround(px) + 2, y0 + 3);
    c.fill_rect(iround(px) + 3, y0 - 6, iround(px) + 12, y0 + 6);
    c.line(px + 13, y0, 127, 56, 2);
    if (plugged && in < on_at + 100 + PL_SPARK) {   // sparks at the socket
        float sp = seg(in, on_at + 100, on_at + 100 + PL_SPARK);
        for (int k = 0; k < 5; k++) {
            float r0 = 4 + 6 * sp;
            float dx = UC[(k * 2 + 12) % 16], dy = US[(k * 2 + 12) % 16];
            c.line(92 + r0 * dx, 32 + r0 * dy, 92 + (r0 + 3) * dx, 32 + (r0 + 3) * dy);
        }
    }
}

// ---------------------------------------------------------------------- squish

static void wall(Canvas& c, int x0, int x1) {
    if (x1 < x0) return;
    c.fill_rect(x0, 0, x1, 63);
    for (int y = 7; y < 64; y += 8) c.hline(x0, x1, y, false);
    for (int y = 0; y < 64; y += 8) {
        int off = (y / 8) % 2 ? 4 : 0;
        for (int xx = (x0 / 8) * 8 + off; xx <= x1; xx += 8)
            if (xx >= x0) c.vline(xx, y, y + 6, false);
    }
}

static void a_squish(Canvas& c, uint32_t t, const Ctx& x) {
    const Key k[] = {{0, 0}, {300, 0}, {900, 1}, {1800, 1}, {2100, 0}};
    float s = KEYS(k, t);
    float shake = (t > 900 && t < 1800) ? sinf(t / 23.0f) * 0.8f : 0;
    float L = -1 + 42 * s + shake;   // wall inner edge
    wall(c, 0, iround(L));
    wall(c, 127 - iround(L), 127);
    Eye l, r;
    r.mirror = true;
    if (t < 2100) {
        float xo = fmaxf(36, L + 2);   // outer edge pushed by the wall
        float xi = 48 + 3 * s;          // inner edge squeezed a bit too
        float w = fmaxf(4, xi - xo);
        l.w = r.w = w;
        l.h = r.h = fminf(36, EYE_H * EYE_W / w);
        l.dx = (xo + xi) / 2 - LEFT_X;
        r.dx = -l.dx;
        face(c, l, r);
        if (t > 1100 && t < 1800 && (x.seed & 1)) sweat(c, 64, 6 + 10 * seg(t, 1100, 1800));
        return;
    }
    // pop back with a wobble, then blink
    const Key w[] = {{2100, 4}, {2220, 15}, {2340, 11}, {2440, 12}};
    const Key hh[] = {{2100, 36}, {2220, 18}, {2340, 23}, {2440, 22}};
    Eye e;
    e.w = KEYS(w, t);
    e.h = KEYS(hh, t);
    face_blink(c, t, 2600, e);
}

static const Anim A_WORKING = {"working", a_working, 3 * WK_CYC, LOOP};
static const Anim A_SEARCH = {"searching", a_searching, 9000, LOOP};
static const Anim A_NEEDS = {"needs_you", a_needs_you, 8640, LOOP};
static const Anim A_DONE = {"done", a_done, 2300, CALM};
static const Anim A_PARTY = {"party", a_party, 3300, CALM};
static const Anim A_READING = {"reading", a_reading, 10000, LOOP};
static const Anim A_TYPING = {"typing", a_typing, 10000, LOOP};
static const Anim A_TERMINAL = {"terminal", a_terminal, 12000, LOOP};
static const Anim A_WEB = {"web", a_web, 11100, LOOP};
static const Anim A_TODO = {"todo", a_todo, 3400, CALM};
static const Anim A_PLUG = {"plug", a_plug, 9000, LOOP};
static const Anim A_SQUISH = {"squish", a_squish, 3100, CALM};

ANIM_LIST(anims_work, &A_WORKING, &A_SEARCH, &A_NEEDS, &A_DONE, &A_PARTY, &A_READING, &A_TYPING,
          &A_TERMINAL, &A_WEB, &A_TODO, &A_PLUG, &A_SQUISH);
