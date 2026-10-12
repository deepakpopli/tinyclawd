// more small moments: coffee, a rubber duck, stretching, juggling, a bug,
// a git branch, a paper plane and a night sky
#include <string.h>

#include "anim.h"

static const float PI = (float)M_PI;

// ------------------------------------------------------------------- helpers

// tiny pixel art from strings, '#' = on. flip mirrors it (rows must be the same width)
static void art(Canvas& c, int x, int y, const char* const* rows, int n, bool flip = false, bool on = true) {
    for (int r = 0; r < n; r++) {
        int w = strlen(rows[r]);
        for (int i = 0; i < w; i++)
            if (rows[r][i] == '#') c.px(flip ? x + w - 1 - i : x + i, y + r, on);
    }
}

// which way a scene faces, m = mirrored
struct Side {
    bool m;
    int px(int v) const { return m ? 127 - v : v; }
    int box(int v, int w) const { return m ? 128 - w - v : v; }   // left edge of something w wide
    Eyes look(bool fwd) const { return fwd != m ? EYES_RIGHT : EYES_LEFT; }
};

// ------------------------------------------------------------------- coffee

static const int8_t WIG[] = {0, 1, 1, 0, -1, -1};

// two wisps of steam, the wiggle travels up
static void steam(Canvas& c, int x, int y, uint32_t t, uint32_t seed, float blow) {
    for (int k = 0; k < 2; k++) {
        int ph = t / 110 + k * 3 + (seed & 3);
        int len = 5 + (hash32((t / 1300) * 3 + k + seed) & 3);
        for (int i = 0; i < len; i++) {
            int xx = x + k * 7 + WIG[((i - ph) % 6 + 6) % 6] + iround(blow * i);
            int yy = y - i * 2;
            if (i == len - 1 && (t / 110) % 2) continue;   // the tip flickers
            c.px(xx, yy);
            c.px(xx, yy - 1);
        }
    }
}

static const char* const MUG_HEART[] = {"##.##", "#####", ".###.", "..#.."};

// mug, top-left at x, top; sh tilts the top sideways
static void mug(Canvas& c, int x, int top, float sh) {
    const int w = 18, h = 14;
    float p[] = {x + sh, (float)top, x + w - 1 + sh, (float)top, (float)(x + w - 1), (float)(top + h - 1),
                 (float)x, (float)(top + h - 1)};
    c.fill_poly(p, 4);
    int s = iround(sh * 0.5f);
    c.hline(x + 1 + iround(sh), x + w - 2 + iround(sh), top + 2, false);
    c.rect(x + w + s, top + 3, x + w + 3 + s, top + 9);
    c.vline(x + w + 1 + s, top + 3, top + 9);
    art(c, x + 7 + s, top + 6, MUG_HEART, 4, false, false);
}

static void a_coffee(Canvas& c, uint32_t t, const Ctx& x) {
    bool blow = x.seed & 1, twice = x.seed & 2;
    uint32_t end = twice ? 5300 : 4300;
    // mug height: below the screen -> resting -> up for a sip
    const Key in[] = {{300, 64}, {800, 46}};
    const Key out[] = {{end, 46}, {end + 450, 66}};
    float top = t < end ? KEYS(in, t) : KEYS(out, t);
    float sh = 0, lid = 0;
    bool happy = false;
    uint32_t sips[2] = {1800u, twice ? 3900u : 99999u};
    for (uint32_t a : sips) {
        if (t < a || t >= a + 1100) continue;
        const Key up[] = {{a, 46}, {a + 260, 36}, {a + 820, 36}, {a + 1100, 46}};
        const Key tilt[] = {{a + 150, 0}, {a + 350, -4}, {a + 700, -4}, {a + 900, 0}};
        const Key cl[] = {{a + 100, 0}, {a + 250, 1}, {a + 850, 1}, {a + 1000, 0.3f}};
        top = KEYS(up, t);
        sh = KEYS(tilt, t);
        lid = KEYS(cl, t);
    }
    if ((t >= 2900 && t < 3800) || (twice && t >= 5000 && t < 5300)) happy = true;
    Eye e;
    float look = clamp01((64 - top) / 18);
    e.dy = 3 * look;
    e.h = EYE_H - 2 * look;
    e.lid = lid;
    if (blow && t >= 1100 && t < 1650) e.lid = 0.35f * sinf(PI * seg(t, 1100, 1650));
    if (happy) {
        e.shape = HAPPY;
        e.dy = 2 + breathe(t, 600);
        face(c, e);
        if (t >= 2950 && t < 3650) c.text_center(64, 1, "ahh", font_5x7);
    } else if (t >= 3800 && t < 3900) {
        face(c, blink_eye(e, 0.5f + 0.5f * seg(t, 3800, 3900)));
    } else {
        face_blink(c, t, twice ? 3500 : 99999, e);
    }
    if (top < 64) {
        float b = blow && t >= 1100 && t < 1650 ? 0.9f * sinf(PI * seg(t, 1100, 1650)) : 0;
        if (fabsf(sh) < 1) steam(c, 59 + iround(b * 2), iround(top) - 3, t, x.seed, b);
        mug(c, 55, iround(top), sh);
    }
}

// --------------------------------------------------------------- rubber duck

static const char* const DUCK[] = {
    ".....####.........",
    "....######........",
    "...##.#####.......",
    ".##########.......",
    "###########.......",
    "..#########.......",
    "....#######......#",
    "...############.##",
    "..###############.",
    "..######....#####.",
    "..#######..######.",
    "..###############.",
    "...#############..",
    "....###########...",
};

// a little bounce, 300 ms per hop
static float hop(uint32_t u, float h = 4) {
    return h * sinf(PI * (u % 300) / 300.0f);
}

// rounded box with a little tail pointing down at tx
static void bubble_box(Canvas& c, int x0, int y0, int x1, int y1, int tx) {
    c.fill_rect(x0, y0, x1, y1, false);
    c.rect(x0, y0, x1, y1);
    c.px(x0, y0, false);
    c.px(x1, y0, false);
    c.px(x0, y1, false);
    c.px(x1, y1, false);
    c.hline(tx, tx + 3, y1, false);
    c.line(tx, y1, tx - 2, y1 + 4);
    c.line(tx + 3, y1, tx - 2, y1 + 4);
}

static const char* const SAY[] = {"so this fn", "then x is 0", "it loops", "wait...", "line 42",
                                  "it returns", "but why", "if null"};
static const char* const AHA[] = {"aha!", "got it!", "oh! null", "a typo!"};

static void a_rubber_duck(Canvas& c, uint32_t t, const Ctx& x) {
    Side s{(x.seed & 4) != 0};
    const int GY = 58, CX = 10, CY = 38, DX = 80;
    c.hline(0, 127, GY);
    uint32_t h = hash32(x.seed + 77);
    bool talk = t >= 500 && t < 3200;
    bool aha = t >= 4000 && t < 4600;
    // clawd
    Pose p = pose(s.look(true));
    int y = CY;
    if (talk) {
        p.arms = (t / 220) % 3 == 1 ? ARMS_ONE_UP : ARMS_DOWN;
        y -= (t / 160) % 2;
    }
    if (aha) {
        p.arms = ARMS_UP;
        y -= iround(hop(t - 4000, 6));
    }
    if (t >= 4600) {
        p.eyes = t < 5600 ? EYES_CLOSED : s.look(true);
        y -= iround(hop(t - 4600, 2)) * (t < 5500);
    }
    clawd(c, s.box(CX, SPRITE_W), y, p);
    // duck: tilts its head while listening
    bool tilt = t >= 3300 && t < 3900;
    art(c, s.box(DX, 18), GY - 14 + tilt, DUCK, 14, s.m);
    if (tilt) c.px(s.px(DX + 5), GY - 12 + 1);   // blink
    // speech bubble
    char buf[16];
    const char* line = nullptr;
    if (talk) line = SAY[(h + ((t - 500) / 900) * 3) % 8];
    else if (t >= 3200 && t < 4000) {
        int n = 1 + (t - 3200) / 250;
        memcpy(buf, "...", 3);
        buf[n > 3 ? 3 : n] = 0;
        line = buf;
    } else if (t >= 4600 && t < 5600) line = AHA[(h >> 8) % 4];
    if (line) {
        int w = c.text_width(line) + 8;
        if (w < 22) w = 22;
        int x0 = 30, x1 = x0 + w;
        bool pop = (talk && (t - 500) % 900 < 70) || (t >= 4600 && t < 4670);
        if (pop) x1 -= 4;
        int ax0 = s.m ? 127 - x1 : x0, ax1 = s.m ? 127 - x0 : x1;
        bubble_box(c, ax0, 4, ax1, 18 - pop, s.px(CX + 26) - (s.m ? 3 : 0));
        c.text(ax0 + 4 + (pop ? -2 : 0), 8 - pop, line);
    }
    if (aha) c.text(s.box(CX + 14, 4), 14 - iround(hop(t - 4000, 6)), "!", font_bold16);
    // thank you duck: a heart and a squeak
    if (t >= 4800 && t < 6000) heart(c, s.px(DX + 9), GY - 20 - 10 * seg(t, 4800, 6000), 0.55f);
    if (t >= 5000 && t < 5400 && (t / 100) % 2) {
        int bx = s.px(DX - 2), d = s.m ? 1 : -1;
        c.line(bx, GY - 12, bx + 3 * d, GY - 14);
        c.line(bx, GY - 10, bx + 4 * d, GY - 10);
    }
}

// ------------------------------------------------------------------- stretch

static void a_stretch(Canvas& c, uint32_t t, const Ctx& x) {
    float lean = (x.seed & 8) ? 1 : -1;
    bool sway = x.seed & 16;
    // squash down, then a long stretch up, wobble, release
    const Key dy[] = {{300, 0}, {800, 5}, {1150, -9}, {2700, -10}, {2900, 4}, {3100, -1}, {3300, 0}};
    const Key w[] = {{300, 12}, {800, 15}, {1150, 7}, {2700, 6}, {2900, 16}, {3100, 11}, {3300, 12}};
    const Key hh[] = {{300, 22}, {800, 16}, {1150, 36}, {2700, 39}, {2900, 15}, {3100, 23}, {3300, 22}};
    Eye e;
    e.dy = KEYS(dy, t);
    e.w = KEYS(w, t);
    e.h = KEYS(hh, t);
    bool top = t >= 1250 && t < 2700;
    if (top) {
        // trembling at the top, and a slow lean to one side
        e.dx = ((t / 45) % 2 ? 0.6f : -0.6f);
        if (sway) e.dx += 8 * lean * sinf(PI * seg(t, 1400, 2600));
    }
    if (t >= 3300 && t < 4100) {   // that felt good
        Eye hp;
        hp.shape = HAPPY;
        hp.dy = 2;
        face(c, hp);
    } else if (t >= 4100 && t < 4200) {
        face(c, blink_eye(Eye(), 0.5f + 0.5f * seg(t, 4100, 4200)));
    } else {
        face(c, e);
    }
    // a pop when it lets go
    if (t >= 2750 && t < 3050) {
        int r = 3 + iround(4 * seg(t, 2750, 3050));
        sparkle(c, 14, 10, r > 5 ? 5 : r);
        sparkle(c, 113, 10, r > 5 ? 5 : r);
    }
}

// -------------------------------------------------------------------- juggle

static const char* const BLK[3][5] = {
    {".###.", "#####", "#####", "#####", ".###."},
    {"#####", "#...#", "#...#", "#...#", "#####"},
    {"..#..", ".###.", "#####", ".###.", "..#.."},
};

struct Juggle {
    uint32_t start, beat;
    int throws, high;   // number of throws, the one thrown extra high
    bool flub;
};

static Juggle juggle_of(uint32_t seed) {
    uint32_t h = hash32(seed + 404);
    Juggle j;
    j.start = 600;
    j.beat = 250 + h % 60;
    j.throws = 10 + (h >> 8) % 3;
    j.high = (h >> 12) & 1 ? 4 + (h >> 14) % 4 : -1;
    j.flub = (h >> 20) % 3 == 0;
    return j;
}

static const int JX = 46, JY = 36;

// block k position, returns true while it is in the air
static bool block_at(const Juggle& j, int k, uint32_t t, float& bx, float& by) {
    const float hx[2] = {JX - 3, JX + 38}, hy = JY + 5;
    float tb = t < j.start ? -1 : (t - j.start) / (float)j.beat;
    if (tb < k) {   // waiting in the first hand, two stacked on the left
        bx = hx[k % 2];
        by = hy - (k == 2 ? 6 : 0);
        return false;
    }
    int n = k + 3 * (int)((tb - k) / 3);
    if (n >= j.throws) n -= 3 * ((n - j.throws) / 3 + 1);   // the last real throw
    float u = tb - n;
    int from = n % 2;
    float fx = hx[from], tx = hx[1 - from], ty = hy, apex = 10;
    bool last = n + 3 >= j.throws;
    if (last) {   // goes onto the head, in throw order
        tx = JX + 18;
        ty = JY - 3 - 5 * (n - (j.throws - 3));
        apex = 6;
    }
    if (n == j.high) apex = -4;
    if (u < 2) {
        float p = u / 2;
        bx = lerp(fx, tx, p);
        by = lerp(hy, ty, p) - (lerp(hy, ty, p) - apex) * 4 * p * (1 - p);
        return true;
    }
    bx = tx;
    by = ty;
    if (!last) by += 2 * sinf(PI * (u - 2));   // little scoop in the hand
    return false;
}

static void a_juggle(Canvas& c, uint32_t t, const Ctx& x) {
    Juggle j = juggle_of(x.seed);
    uint32_t done = j.start + (j.throws + 1) * j.beat;   // all on the head
    c.hline(0, 127, JY + 21);
    float bx[3], by[3];
    float hi = 99, hx = 64;
    for (int k = 0; k < 3; k++) {
        if (block_at(j, k, t, bx[k], by[k]) && by[k] < hi) {
            hi = by[k];
            hx = bx[k];
        }
    }
    Pose p;
    p.eyes = hx < JX + 12 ? EYES_LEFT : (hx > JX + 24 ? EYES_RIGHT : EYES_OPEN);
    float tb = (t - j.start) / (float)j.beat;
    if (t >= j.start && tb < j.throws && (int)tb % 2 && tb - (int)tb < 0.4f) p.arms = ARMS_ONE_UP;
    uint32_t fall = done + 500;
    if (t >= done) {
        if (!j.flub) {   // ta da
            p.arms = t >= done + 200 ? ARMS_UP : ARMS_DOWN;
            p.eyes = t >= done + 200 && t < done + 1200 ? EYES_CLOSED : EYES_OPEN;
            if (t >= done + 200 && t < done + 1400) {
                sparkle(c, 30, 14 + ((t / 150) % 2) * 4);
                sparkle(c, 98, 18 - ((t / 150) % 2) * 4);
            }
        } else if (t < fall) {   // the tower wobbles
            float w = (t - done) / 500.0f;
            for (int k = 0; k < 3; k++) {
                int lvl = iround((JY - 3 - by[k]) / 5);
                bx[k] += lvl * 1.5f * sinf(w * 3 * PI) * w;
            }
            p.eyes = EYES_LEFT;
        } else {   // and falls
            float u = (t - fall) / 1000.0f;
            for (int k = 0; k < 3; k++) {
                int lvl = iround((JY - 3 - by[k]) / 5);
                float d = (lvl == 1 ? -1 : 1) * (24 + 8 * lvl);
                float g = clamp01(u * 2.2f - lvl * 0.15f);
                bx[k] += d * g;
                by[k] = fminf(by[k] - 6 * sinf(PI * g * 0.6f) + 30 * g * g, JY + 17);
            }
            p.eyes = EYES_CLOSED;
            if (t >= fall + 200 && t < fall + 1600) stars(c, JX + 18, JY - 4, (t - fall) / 600.0f);
        }
    }
    clawd(c, JX, JY, p);
    for (int k = 0; k < 3; k++) {   // a dark border so they read on top of him
        int bx0 = iround(bx[k]) - 2, by0 = iround(by[k]) - 2;
        c.fill_rect(bx0 - 1, by0 - 1, bx0 + 5, by0 + 5, false);
        art(c, bx0, by0, BLK[k], 5);
    }
}

static const Anim A_COFFEE = {"coffee", a_coffee, 5800, CALM};
static const Anim A_DUCK = {"rubber_duck", a_rubber_duck, 6200, SCENE};
static const Anim A_STRETCH = {"stretch", a_stretch, 4400, CALM};
static const Anim A_JUGGLE = {"juggle", a_juggle, 7000, SCENE};

ANIM_LIST(anims_more, &A_COFFEE, &A_DUCK, &A_STRETCH, &A_JUGGLE);
