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

// ---------------------------------------------------------------- bug squash

static const char* const BUG[2][6] = {
    {".......#.#", "...####.#.", "..#######.", ".########.", ".#######..", "#.#.#.#..."},
    {".......#.#", "...####.#.", "..#######.", ".########.", ".#######..", ".#.#.#.#.."},
};

static void a_bug_squash(Canvas& c, uint32_t t, const Ctx& x) {
    Side s{(x.seed & 32) != 0};
    bool miss = x.seed & 64;
    const int GY = 59, CY = 39;
    c.hline(0, 127, GY);
    // bug: crawls in, stops, and on a miss dashes under Clawd
    const Key bk[] = {{0, -12}, {1700, 28}};
    float bug = KEYS(bk, t);
    uint32_t squash = miss ? 3300 : 2250;
    if (miss && t >= 1950) bug = lerp(28, 62, ease(seg(t, 1950, 2250)));
    // clawd jumps from cx0 to land on the bug
    float cx = 76, cy = CY;
    float t1 = 28 + 5 - 18, t2 = 62 + 5 - 18;
    if (t >= 1750) {
        float u = seg(t, 1750, 2250);
        cx = lerp(76, t1, ease(u));
        cy = CY - 16 * sinf(PI * u);
    }
    if (miss && t >= 2850) {
        float u = seg(t, 2850, 3300);
        cx = lerp(t1, t2, ease(u));
        cy = CY - 12 * sinf(PI * u);
    }
    bool flat = t >= squash;
    Pose p = pose(s.look(false));
    if (t >= 1750 && !flat) p = pose(EYES_OPEN, ARMS_UP, FEET_NONE);
    if (miss && t >= 2250 && t < 2850) p = pose((t / 250) % 2 ? EYES_LEFT : EYES_RIGHT);
    if (miss && t >= 2650 && t < 2850) p = pose(s.look(true));
    int shake = 0;
    if (flat) {
        uint32_t q = t - squash;
        shake = q < 160 ? ((q / 40) % 2 ? 1 : -1) : 0;
        p = pose(q < 500 ? EYES_CLOSED : EYES_OPEN, q >= 500 && q < 1700 ? ARMS_UP : ARMS_DOWN);
        if (q >= 500 && q < 1700) cy -= iround(hop(q - 500, 4));
        if (q >= 1500) {   // steps back to show the smear
            float u = seg(q, 1500, 2000);
            cx += 22 * ease(u);
            if (u < 1) p.feet = (q / 90) % 2 ? FEET_LEFT : FEET_RIGHT;
            p.eyes = s.look(false);
        }
    }
    clawd(c, s.box(iround(cx), SPRITE_W) + shake, iround(cy), p);
    if (!flat) {
        art(c, s.box(iround(bug), 10), GY - 6, BUG[(t / 120) % 2], 6, s.m);
    } else {
        // squashed flat under him, dust puffs out both sides
        uint32_t q = t - squash;
        float b = miss ? 62 : 28;
        if (q < 450) {
            float d = 4 + 14 * ease_out(q / 450.0f);
            for (int k = -1; k <= 1; k += 2) {
                int dx = s.px(iround(b + 5 + k * (d + 12)));
                c.fill_rect(dx - 1, GY - 3 - iround(q / 150.0f), dx, GY - 2 - iround(q / 150.0f));
                c.px(dx + k * 3, GY - 5);
            }
        }
        if (q >= 600 && q < 1900) {
            static const char* const WORDS[] = {"FIXED!", "SQUASH!", "PASS", "GOT IT"};
            c.text_center(s.px(64), 4, WORDS[hash32(x.seed + 9) % 4], font_bold12);
        }
        if (q >= 1500) {   // the bug is a flat little smear
            int bx = s.box(iround(b) - 1, 12);
            c.hline(bx, bx + 11, GY - 1);
            c.px(bx + 2, GY - 2);
            c.px(bx + 6, GY - 2);
            c.px(bx + 9, GY - 2);
        }
    }
    if (t >= 900 && t < 1600 && (t / 120) % 4) c.text(s.box(iround(cx) + 16, 4), CY - 18, "!", font_bold16);
    if (miss && t >= 2300 && t < 2700) c.text(s.box(iround(cx) + 15, 6), CY - 14, "?", font_6x10);
}

// ---------------------------------------------------------------- git branch

// polyline drawn up to length len (segments are flat or steep, length = longest side)
static void grow(Canvas& c, const int* p, int n, float len, float& tx, float& ty) {
    tx = p[0];
    ty = p[1];
    for (int i = 1; i < n && len > 0; i++) {
        float dx = p[2 * i] - p[2 * i - 2], dy = p[2 * i + 1] - p[2 * i - 1];
        float l = fmaxf(fabsf(dx), fabsf(dy));
        float f = len >= l ? 1 : len / l;
        tx = p[2 * i - 2] + dx * f;
        ty = p[2 * i - 1] + dy * f;
        c.line(p[2 * i - 2], p[2 * i - 1], tx, ty);
        len -= l;
    }
}

// a commit dot that pops in with a little overshoot
static void commit(Canvas& c, int x, int y, uint32_t t, uint32_t at) {
    if (t < at) return;
    float r = t < at + 120 ? 3 : 2;
    c.fill_circle(x, y, r);
}

static const char* const BRANCH[] = {"fix", "feat", "wip", "bugfix", "try"};

static void a_git_branch(Canvas& c, uint32_t t, const Ctx& x) {
    uint32_t h = hash32(x.seed + 31);
    const int MY = 30, BY = 10, B0 = 22, M = 92;
    int nb = 2 + h % 3;           // commits on the branch
    bool main_commit = h & 8;     // someone else committed to main meanwhile
    const int br[] = {B0, MY, B0 + 10, BY, M - 10, BY, M, MY};
    const uint32_t B_AT = 700, B_END = 2700;   // branch grows
    const float B_LEN = 20 + (M - B0 - 20) + 20;
    // main: to the branch point, slowly to the merge, then on
    float mlen;
    if (t < B_AT) mlen = (B0 - 4) * ease(seg(t, 150, B_AT));
    else if (t < B_END) mlen = (B0 - 4) + (M - B0) * seg(t, 900, B_END - 150);
    else mlen = (M - 4) + 28 * ease_out(seg(t, 2950, 3500));
    const int mn[] = {4, MY, 124, MY};
    float mx, my, bx, by;
    grow(c, mn, 2, mlen, mx, my);
    float blen = B_LEN * seg(t, B_AT, B_END);
    grow(c, br, 4, blen, bx, by);
    // commits on main
    commit(c, 8, MY, t, 200);
    commit(c, B0, MY, t, B_AT - 100);
    if (main_commit) commit(c, 56, MY, t, 900 + (uint32_t)((56 - B0) / (float)(M - B0) * (B_END - 1050)));
    // commits on the branch
    for (int i = 0; i < nb; i++) {
        int cx = B0 + 18 + i * (M - B0 - 36) / (nb > 1 ? nb - 1 : 1);
        float at_len = 20 + (cx - B0 - 10);
        commit(c, cx, BY, t, B_AT + (uint32_t)(at_len / B_LEN * (B_END - B_AT)));
    }
    // merge: a hollow dot that pops
    if (t >= B_END) {
        uint32_t q = t - B_END;
        int r = q < 150 ? 4 : 3;
        c.fill_circle(M, MY, r);
        c.fill_circle(M, MY, r - 1.5f, false);
        if (q < 350) {
            int d = 5 + iround(8 * ease_out(q / 350.0f));
            for (int k = -1; k <= 1; k += 2) {
                c.px(M + k * d, MY);
                c.px(M, MY + k * d);
                c.px(M + k * (d - 2), MY + k * (d - 2));
                c.px(M + k * (d - 2), MY - k * (d - 2));
            }
        }
    }
    if (t >= 3500) commit(c, 122, MY, t, 3500);
    c.text(4, MY + 4, "main", font_4x6);
    if (t >= B_AT + 300) c.text(B0 + 12, BY - 8, BRANCH[(h >> 4) % 5], font_4x6);
    // clawd watching the tip, then a happy hop
    float tip = t >= B_AT && t < B_END ? bx : mx;
    Pose p = pose(tip < 48 ? EYES_LEFT : (tip > 74 ? EYES_RIGHT : EYES_OPEN));
    int y = 39;
    if (t >= B_END + 50 && t < B_END + 1250) {
        p = pose(EYES_CLOSED, ARMS_UP);
        y -= iround(hop(t - B_END - 50, 5));
    }
    clawd(c, 28, y, p);
    if (t >= B_END + 300) {
        c.text(80, 42, "merged", font_5x7);
        char hash[8];
        for (int i = 0; i < 7; i++) hash[i] = "0123456789abcdef"[(hash32(x.seed * 7 + i) >> 3) & 15];
        hash[7] = 0;
        if (t >= B_END + 600) c.text(84, 52, hash, font_4x6);
    }
}

// --------------------------------------------------------------- paper plane

static const char* const SHEET[] = {"##########", "##########", "##########", "##########",
                                    "##########", "##########", "##########", "##########"};
static const char* const HALF_FOLD[] = {"##########", ".#########", "..########", "...#######",
                                        "....######"};

// a plane at x, y heading along ux, uy (unit), screen y points down
static void plane(Canvas& c, float x, float y, float ux, float uy) {
    float nx = uy, ny = -ux;   // "up" for the plane
    float p[] = {x + 9 * ux, y + 9 * uy, x - 7 * ux + 6 * nx, y - 7 * uy + 6 * ny,
                 x - 7 * ux - 3 * nx, y - 7 * uy - 3 * ny};
    c.fill_poly(p, 3);
    c.line(x + 8 * ux, y + 8 * uy, x - 6 * ux + nx, y - 6 * uy + ny, 1, false);
}

static const float PX0 = 44, PY0 = 28;

static void plane_path(int kind, float d, float& x, float& y) {
    if (kind == 1) {   // loop the loop
        const float LY = PY0 + 4;   // bottom of the loop
        if (d < 600) {
            x = PX0 + d * 0.04f;
            y = PY0 + 4 * ease_out(d / 600.0f);
        } else if (d < 1500) {
            float a = 2 * PI * (d - 600) / 900;
            x = PX0 + 24 + 10 * sinf(a);
            y = LY - 10 + 10 * cosf(a);
        } else {
            x = PX0 + 24 + (d - 1500) * 0.08f;
            y = LY - 4 * ease_out((d - 1500) / 600.0f);
        }
        return;
    }
    if (kind == 2) {   // nose dive into the floor
        d = fminf(d, 1150);
        x = PX0 + 0.06f * d;
        y = PY0 - 0.03f * d + 0.000045f * d * d;
        return;
    }
    x = PX0 + d * 0.07f;   // a long glide
    y = PY0 - 12 * ease_out(d / 900.0f) + 3 * sinf(d / 260.0f);
}

static void a_paper_plane(Canvas& c, uint32_t t, const Ctx& x) {
    Side s{(x.seed & 128) != 0};
    int kind = hash32(x.seed + 5) % 3;
    const int CX = 6, CY = 38, GY = 59;
    const uint32_t THROW = 1800;
    c.hline(0, 127, GY);
    Pose p = pose(s.look(true));
    int y = CY;
    // folding: sheet, half, plane, each with a little bounce
    if (t < THROW) {
        int st = t < 600 ? 0 : (t < 1000 ? 1 : 2);
        if (t >= 400 && t < 1200 && (t % 400) < 120) y -= 1;
        if (t >= 1450) p.arms = ARMS_ONE_UP;
        float back = t >= 1450 ? 3 * ease(seg(t, 1450, THROW)) : 0;
        float lift = t >= 1450 ? 8 * ease(seg(t, 1450, THROW)) : 0;
        int px0 = CX + 30 - iround(back), py0 = CY - 10 - iround(lift);
        if (st == 0) art(c, s.box(px0, 10), py0, SHEET, 8, s.m);
        else if (st == 1) art(c, s.box(px0, 10), py0 + 2, HALF_FOLD, 5, s.m);
        else plane(c, s.px(px0 + 5), py0 + 4, s.m ? -1 : 1, 0);
        if (st > 0 && t < (st == 1 ? 700u : 1100u)) sparkle(c, s.px(px0 + 12), py0 - 2, 2);
        clawd(c, s.box(CX, SPRITE_W), y, p);
        return;
    }
    float d = t - THROW;
    float fx, fy, gx, gy;
    plane_path(kind, d, fx, fy);
    plane_path(kind, fminf(d + 25, kind == 2 ? 1120 : 99999), gx, gy);
    float ux = gx - fx, uy = gy - fy, l = sqrtf(ux * ux + uy * uy);
    if (l < 0.01f) {
        ux = 1;
        uy = 1;
        l = 1.414f;
    }
    ux /= l;
    uy /= l;
    bool landed = kind == 2 && d >= 1150;
    if (fx < 140) {
        // dotted trail
        if (!landed)
            for (int k = 1; k <= 2; k++) {
                float tx, ty;
                plane_path(kind, fmaxf(0, d - k * 70), tx, ty);
                c.px(s.px(iround(tx)), iround(ty));
            }
        plane(c, s.px(iround(fx)), fy, s.m ? -ux : ux, uy);
    }
    // clawd: arm down after the throw, then reacts
    p.arms = d < 150 ? ARMS_ONE_UP : ARMS_DOWN;
    if (landed) {
        uint32_t q = d - 1150;
        if (q < 300) {   // bonk
            int bx = s.px(iround(fx) + 6);
            c.line(bx, GY - 10, bx + (s.m ? -3 : 3), GY - 13);
            c.line(bx + (s.m ? -2 : 2), GY - 6, bx + (s.m ? -6 : 6), GY - 6);
        }
        if (q >= 300) {
            p.eyes = EYES_CLOSED;
            sweat(c, s.px(CX + 36), CY - 8 + 6 * seg(q, 300, 1500));
        }
    } else if (fx > 128 || (kind == 1 && d > 600 && d < 1500)) {
        bool yay = fx > 128 && d < (kind == 1 ? 3300 : 2600);
        p.arms = yay ? ARMS_UP : ARMS_DOWN;
        if (yay) {
            y -= iround(hop(t, 4));
            p.eyes = EYES_CLOSED;
        }
    }
    clawd(c, s.box(CX, SPRITE_W), y, p);
}

static const Anim A_COFFEE = {"coffee", a_coffee, 5800, CALM};
static const Anim A_DUCK = {"rubber_duck", a_rubber_duck, 6200, SCENE};
static const Anim A_STRETCH = {"stretch", a_stretch, 4400, CALM};
static const Anim A_JUGGLE = {"juggle", a_juggle, 7000, SCENE};
static const Anim A_BUG = {"bug_squash", a_bug_squash, 5800, SCENE};
static const Anim A_GIT = {"git_branch", a_git_branch, 5200, SCENE};
static const Anim A_PLANE = {"paper_plane", a_paper_plane, 5000, SCENE};

ANIM_LIST(anims_more, &A_COFFEE, &A_DUCK, &A_STRETCH, &A_JUGGLE, &A_BUG, &A_GIT, &A_PLANE);
