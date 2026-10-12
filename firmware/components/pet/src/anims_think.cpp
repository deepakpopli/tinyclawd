// more ways to think, for the moment between Enter and the first tool.
// cheap on purpose: rects, lines, glyphs, tables, a few sinf per frame.
#include <string.h>

#include "anim.h"

static const float PI = (float)M_PI;

// ------------------------------------------------------------------- helpers

// the spinner from the CLI, forward then back
static const uint16_t SPIN[] = {0x00b7, 0x2722, 0x2733, 0x2736, 0x273b, 0x273d};

static uint16_t spin_glyph(uint32_t t, uint32_t step = 110) {
    int i = (t / step) % 12;
    if (i >= 6) i = 11 - i;
    return SPIN[i];
}

static const float COS32[32] = {
    1.000f, 0.981f, 0.924f, 0.831f, 0.707f, 0.556f, 0.383f, 0.195f,
    0.000f, -0.195f, -0.383f, -0.556f, -0.707f, -0.831f, -0.924f, -0.981f,
    -1.000f, -0.981f, -0.924f, -0.831f, -0.707f, -0.556f, -0.383f, -0.195f,
    0.000f, 0.195f, 0.383f, 0.556f, 0.707f, 0.831f, 0.924f, 0.981f};

// p = 0..1 around the circle
static float tcos(float p) { return pickf(COS32, 32, p * 32); }
static float tsin(float p) { return pickf(COS32, 32, p * 32 - 8); }

// the spinner redrawn big with lines: frame i = 0..5 like SPIN, k = size, rot = 0..1 turn
static void big_star(Canvas& c, int cx, int cy, int i, float k, float rot) {
    // spokes, length, width, round tip
    static const uint8_t N[] = {0, 4, 8, 6, 8, 8};
    static const uint8_t L[] = {0, 11, 12, 13, 14, 15};
    static const uint8_t WD[] = {0, 3, 2, 3, 2, 3};
    static const uint8_t TIP[] = {0, 0, 0, 0, 2, 2};
    if (i == 0) {
        int r = iround(2 * k);
        c.fill_rect(cx - r, cy - r, cx + r, cy + r);
        return;
    }
    for (int s = 0; s < N[i]; s++) {
        float p = rot + s / (float)N[i];
        float ex = cx + L[i] * k * tcos(p), ey = cy + L[i] * k * tsin(p);
        c.line(cx, cy, ex, ey, WD[i]);
        if (TIP[i]) c.fill_rect(iround(ex) - 1, iround(ey) - 1, iround(ex) + 1, iround(ey) + 1);
    }
    c.fill_rect(cx - 2, cy - 2, cx + 2, cy + 2);
}

// small pixel art: '#' = on, anything else = skip
static void art(Canvas& c, int x, int y, const char* const* rows, int n, bool on = true) {
    for (int r = 0; r < n; r++)
        for (int q = 0; rows[r][q]; q++)
            if (rows[r][q] == '#') c.px(x + q, y + r, on);
}

static float blink_k(uint32_t t, uint32_t at) {
    return t >= at && t < at + BLINK_MS ? (t - at) / (float)BLINK_MS : -1;
}

static Eye maybe_blink(const Eye& e, float k) { return k >= 0 ? blink_eye(e, k) : e; }

static void eyes_at(Canvas& c, float lx, float rx, float y, const Eye& l, const Eye& r) {
    Eye m = r;
    m.mirror = true;
    draw_eye(c, lx, y, l);
    draw_eye(c, rx, y, m);
}

// ------------------------------------------------------------ thinking words
// the CLI moment: spinner, a thinking verb and dots, eyes reading along

static const char* const VERBS[] = {
    "Pondering", "Brewing", "Noodling", "Percolating", "Clauding", "Cogitating",
    "Musing", "Mulling", "Ruminating", "Simmering", "Vibing", "Thinking",
    "Baking", "Booping", "Cooking", "Crunching", "Doodling", "Hatching",
    "Ideating", "Imagining", "Incubating", "Marinating", "Moseying", "Puzzling",
    "Tinkering", "Whirring", "Wibbling", "Zesting", "Herding", "Honking",
    "Schlepping", "Spelunking", "Sprouting", "Stewing", "Swirling", "Waddling",
    "Wandering", "Contemplating", "Deliberating", "Fermenting", "Finagling", "Kneading",
    "Lollygagging", "Meandering", "Moonwalking", "Reticulating", "Scampering", "Shimmying",
    "Smooshing", "Puttering", "Frolicking", "Levitating", "Combobulating",
};
static const int NVERBS = sizeof(VERBS) / sizeof(VERBS[0]);   // 53, prime, so any stride visits all
static const uint32_t TW_WORD = 2600, TW_TYPE = 45;

static void a_thinking_words(Canvas& c, uint32_t t, const Ctx& x) {
    uint32_t n = t / TW_WORD, in = t % TW_WORD;
    uint32_t stride = 1 + x.seed % (NVERBS - 1);
    const char* w = VERBS[(hash32(x.seed) % NVERBS + n * stride) % NVERBS];
    int len = strlen(w);

    // the line: spinner, gap, word, dots. centred on the full width so it never jumps
    const int DOTS = 11;
    int ww = c.text_width(w, font_6x10);
    int x0 = 64 - (12 + 5 + ww + DOTS) / 2, y = 45;
    c.glyph(x0, y, spin_glyph(t), font_sym14);

    // the word types in, then a shine runs across it now and then
    int shown = in / TW_TYPE + 1;
    if (shown > len) shown = len;
    char buf[16];
    memcpy(buf, w, shown);
    buf[shown] = 0;
    int tx = x0 + 17;
    int end = c.text(tx, y, buf, font_6x10);
    uint32_t sh = (in + 1600) % 1300;
    if (shown == len && sh < 500) {
        // bold a three letter window by drawing it again 1 px over
        int k = sh * (len + 3) / 500 - 2;
        for (int i = k; i < k + 3; i++) {
            if (i < 0 || i >= len) continue;
            char one[2] = {w[i], 0};
            int gx = tx + c.text_width(buf, font_6x10) * i / len;
            c.text(gx + 1, y, one, font_6x10);
        }
    }
    if (shown == len) {
        int dots = (in / 320) % 4;
        for (int i = 0; i < dots; i++) c.fill_rect(end + 1 + i * 4, y + 7, end + 2 + i * 4, y + 8);
    } else {
        c.fill_rect(end, y + 1, end + 1, y + 8);   // cursor while typing
    }

    // eyes: follow the cursor while it types, then glance up and wait
    float typing = shown < len ? (float)(in % TW_TYPE + (shown - 1) * TW_TYPE) / (len * TW_TYPE) : 1;
    Eye e;
    e.w = 11;
    e.h = 17;
    e.dy = -8;
    float back = ease(seg(in, len * TW_TYPE + 250, len * TW_TYPE + 650));
    float read = lerp(-9, 9, typing);
    e.dx = lerp(read, 0, back);
    e.dy += 2 * (1 - back);   // looking down at the text
    // a slow look up and around while waiting
    uint32_t h = hash32(n * 31 + x.seed);
    if (h & 1) {
        float up = sinf(PI * seg(in, 1200, 2300));
        e.dy -= 3 * up;
        e.dx += ((h & 2) ? 6 : -6) * up;
    }
    e = maybe_blink(e, blink_k(in, 1000 + (h >> 8) % 1200));
    face(c, e);
}

// --------------------------------------------------------------- big spinner
// the spinner huge in the middle, breathing, eyes watching it from the sides

static void a_big_spinner(Canvas& c, uint32_t t, const Ctx& x) {
    float br = sinf(2 * PI * (t % 1760) / 1760.0f);
    int i = (t / 130) % 12;
    if (i >= 6) i = 11 - i;
    big_star(c, 64, 31, i, 1 + 0.1f * br, (t % 9000) / 9000.0f);

    // eyes watch it, lean in on the breath, sometimes one squints
    uint32_t cyc = t / 3520, in = t % 3520;
    uint32_t h = hash32(cyc * 97 + x.seed);
    Eye l, r;
    l.w = r.w = 10;
    l.h = r.h = 18;
    float lean = 2 * br;
    l.dx = 2 + lean;
    r.dx = -2 - lean;
    l.dy = r.dy = 1 + breathe(t, 1760);
    float sq = sinf(PI * seg(in, 1400, 2600));
    if (h % 3 == 0) l.h -= 8 * sq;
    else if (h % 3 == 1) r.h -= 8 * sq;
    else {   // both lean up and in, curious
        l.dy -= 3 * sq;
        r.dy -= 3 * sq;
        l.dx += 2 * sq;
        r.dx -= 2 * sq;
    }
    float b = blink_k(in, 2900 + (h >> 4) % 300);
    eyes_at(c, 16, 112, 32, maybe_blink(l, b), maybe_blink(r, b));

    // a spark flies off a tip now and then
    uint32_t sp = t % 880;
    if (hash32(t / 880 + x.seed) % 2 && sp < 400) {
        float k = sp / 400.0f;
        int dir = (hash32(t / 880 * 3 + x.seed) % 4);
        int sx = dir & 1 ? 1 : -1, sy = dir & 2 ? 1 : -1;
        int px = iround(64 + sx * (18 + 10 * k)), py = iround(31 + sy * (14 + 8 * k));
        sparkle(c, px, py, k < 0.6f ? 2 : 1);
    }
}

// ----------------------------------------------------------------- lightbulb
// eyes squeeze, sparks, the bulb flickers and then really comes on

static const char* const BULB[] = {
    ".....#####.....",
    "...##.....##...",
    "..#.........#..",
    ".#...........#.",
    ".#...........#.",
    "#.............#",
    "#.............#",
    "#.............#",
    "#.............#",
    ".#...........#.",
    ".#...........#.",
    "..#.........#..",
    "...#.......#...",
    "....#.....#....",
    "....#######....",
    ".....#####.....",
    "....#######....",
    ".....#####.....",
    "......###......",
};
static const char* const GLOW[] = {
    "...............",
    ".....#####.....",
    "...#########...",
    "..###########..",
    "..###########..",
    ".#############.",
    ".#############.",
    ".#############.",
    ".#############.",
    "..###########..",
    "..###########..",
    "...#########...",
    "....#######....",
    ".....#####.....",
};
static const char* const FILAMENT[] = {
    "...............",
    "...............",
    "...............",
    "...............",
    "...............",
    "....#.....#....",
    "....##...##....",
    "....#.#.#.#....",
    "....#..#..#....",
    ".....#...#.....",
    ".....#...#.....",
    ".....#...#.....",
    ".....#...#.....",
    ".....#...#.....",
};
static const int BULB_H = sizeof(BULB) / sizeof(BULB[0]);
static const int GLOW_H = sizeof(GLOW) / sizeof(GLOW[0]);

static const uint32_t LB_STRAIN = 1600, LB_FLICK = 2300, LB_ON = 3900, LB_CYC = 4600;

static void a_lightbulb(Canvas& c, uint32_t t, const Ctx& x) {
    uint32_t cyc = t / LB_CYC, in = t % LB_CYC;
    uint32_t h = hash32(cyc * 131 + x.seed);
    int bx = 57, by = 0;

    // is the bulb lit? off while straining, stutters, then on
    bool lit = false;
    if (in >= LB_FLICK && in < LB_ON) lit = true;
    else if (in >= LB_STRAIN && in < LB_FLICK) {
        static const uint8_t PAT[] = {1, 0, 0, 1, 0, 1, 1, 0, 0, 0, 1, 0, 1, 1};
        lit = PAT[((in - LB_STRAIN) / 50 + (h & 7)) % 14];
    }

    // eyes: squeezed and shaking a little, then wide open, then easing back
    Eye e;
    float wide = ease_out(seg(in, LB_FLICK, LB_FLICK + 140)) * (1 - ease(seg(in, LB_ON, LB_CYC)));
    float pop = in >= LB_FLICK && in < LB_FLICK + 300 ? sinf(PI * seg(in, LB_FLICK, LB_FLICK + 300)) : 0;
    e.w = lerp(14, 13, wide);
    e.h = lerp(6, 22, wide) + 3 * pop;
    e.dy = lerp(14, 11, wide) - 2 * pop;
    if (in >= 250 && in < LB_FLICK) {
        // squeezed shut > <, shaking a little while trying hard
        e.shape = SQUINT;
        e.dy = 10;
        e.dx = ((in / 70) % 2 ? 1 : -1) * seg(in, 250, LB_STRAIN);
    } else if (in < 250) {
        e.h = lerp(6, 3, seg(in, 0, 250));
    }
    if (wide > 0.9f && in > LB_FLICK + 400) e = maybe_blink(e, blink_k(in, LB_FLICK + 900 + (h >> 4) % 400));
    face(c, e);

    if (lit) {
        art(c, bx, by, GLOW, GLOW_H);
        art(c, bx, by, FILAMENT, GLOW_H, false);
    }
    art(c, bx, by, BULB, BULB_H);

    if (in >= LB_FLICK && in < LB_ON) {
        // rays, pulsing out
        float k = (in - LB_FLICK) % 500 / 500.0f;
        int a = 3 + iround(3 * k);
        int cx = bx + 7, cy = by + 8;
        c.hline(cx - 10 - a, cx - 10, cy);
        c.hline(cx + 10, cx + 10 + a, cy);
        c.line(cx - 9, cy - 6, cx - 9 - a, cy - 6 - a / 2);
        c.line(cx + 9, cy - 6, cx + 9 + a, cy - 6 - a / 2);
        c.line(cx - 9, cy + 6, cx - 9 - a, cy + 6 + a / 2);
        c.line(cx + 9, cy + 6, cx + 9 + a, cy + 6 + a / 2);
        // happy sparkles off to the sides
        if ((in / 180) % 2) {
            sparkle(c, 30, 8, 2);
            sparkle(c, 98, 12, 2);
        } else {
            sparkle(c, 24, 14, 1);
            sparkle(c, 104, 6, 2);
        }
    } else if (in < LB_STRAIN) {
        // little sparks fizzing around the bulb while it is off
        for (int i = 0; i < 2; i++) {
            uint32_t slot = (in + i * 130) / 260;
            uint32_t sh = hash32(slot * 17 + i * 5 + cyc * 3 + x.seed);
            if (in < 300) continue;
            int sx = bx + 7 + ((sh & 1) ? 1 : -1) * (11 + (sh >> 3) % 6);
            int sy = by + 3 + (sh >> 7) % 12;
            sparkle(c, sx, sy, (sh >> 12) % 2 + 1);
        }
    }
}

// ---------------------------------------------------------------- idea orbit
// little shapes circle above the head like ideas, then click together

static void idea_shape(Canvas& c, int kind, int x, int y, int r) {
    switch (kind) {
    case 0: c.fill_rect(x - r, y - r, x + r, y + r); break;
    case 1: {
        float p[] = {(float)x, (float)(y - r - 1), (float)(x + r + 1), (float)(y + r), (float)(x - r - 1), (float)(y + r)};
        c.fill_poly(p, 3);
        break;
    }
    case 2: c.fill_circle(x, y, r); break;
    default:
        c.fill_rect(x - r - 1, y - 1, x + r + 1, y + 1);
        c.fill_rect(x - 1, y - r - 1, x + 1, y + r + 1);
        break;
    }
}

static const uint32_t IO_REV = 2600, IO_CYC = 6400;
static const uint32_t IO_IN = 4600, IO_CLICK = 5200, IO_CALM = 5900;

static void a_idea_orbit(Canvas& c, uint32_t t, const Ctx& x) {
    uint32_t in = t % IO_CYC;
    float dir = (x.seed & 1) ? -1 : 1;
    // radius: grows out, orbits, then spirals in fast
    float rad = ease_out(seg(in, 0, 500)) * (1 - ease_in(seg(in, IO_IN, IO_CLICK)));
    float base = dir * ((t % IO_REV) / (float)IO_REV + 0.6f * ease_in(seg(in, IO_IN, IO_CLICK)));
    int first = x.seed % 4;
    const float CX = 64, CY = 10, RX = 36, RY = 6;

    // eyes look up and follow the first one round, happy when they click
    Eye e;
    e.w = 12;
    e.h = 19;
    e.dy = 8;
    if (in < IO_CLICK) {
        e.dx = 6 * tcos(base) * rad;
        e.dy = 6 - 2 * rad;
    } else if (in < IO_CALM) {
        float k = seg(in, IO_CLICK, IO_CALM);
        e.shape = HAPPY;
        e.w = 10;
        e.dy = 8 - 3 * sinf(PI * k);
    }
    if (in > IO_CALM) e = maybe_blink(e, blink_k(in, IO_CALM + 100));
    face(c, e);

    if (in < IO_CLICK) {
        for (int i = 0; i < 4; i++) {
            float p = base + i * 0.25f;
            float sn = tsin(p);
            int r = sn > 0.3f ? 3 : 2;   // the ones in front look bigger
            idea_shape(c, (first + i) % 4, iround(CX + RX * rad * tcos(p)), iround(CY + RY * rad * sn), r);
        }
    } else if (in < IO_CALM) {
        // click: a big sparkle and four small ones flying out
        float k = seg(in, IO_CLICK, IO_CALM);
        int r = iround(6 * (1 - k)) + 2;
        sparkle(c, 64, 10, r);
        c.fill_rect(63, 9, 65, 11);
        int d = iround(8 + 22 * ease_out(k));
        if (k < 0.8f) {
            sparkle(c, 64 - d, 6, 2);
            sparkle(c, 64 + d, 6, 2);
            sparkle(c, 64 - d / 2, 15, 1);
            sparkle(c, 64 + d / 2, 15, 1);
        }
    }
}

// ---------------------------------------------------------------- vibe think
// headphones on, eyes shut, slow nod, notes drifting out

static void headphones(Canvas& c, float dx, float dy) {
    // the band: half an ellipse from cup to cup, 8 segments from the table
    float p[18];
    for (int k = 0; k <= 8; k++) {
        p[2 * k] = 64 - 42 * COS32[k * 2] + dx;
        p[2 * k + 1] = 26 - 24 * COS32[(k * 2 + 24) % 32] + dy;
    }
    c.polyline(p, 9, 2);
    for (int s = -1; s <= 1; s += 2) {
        int cx = iround(64 + s * 44 + dx), cy = iround(34 + dy);
        c.fill_rect(cx - 5, cy - 10, cx + 5, cy + 10);
        c.vline(cx - s * 2, cy - 7, cy + 7, false);   // a seam so it reads as a cup
    }
}

static void a_vibe_think(Canvas& c, uint32_t t, const Ctx& x) {
    const uint32_t BEAT = 900;
    float b = (t % BEAT) / (float)BEAT;
    float nod = 3 * ease_out(b < 0.25f ? b / 0.25f : 1 - (b - 0.25f) / 0.75f);
    float sway = 3 * tsin((t % (4 * BEAT)) / (4.0f * BEAT));

    Eye e;
    e.shape = HAPPY;
    e.w = 10;
    e.dx = sway;
    e.dy = 4 + nod;
    face(c, e);
    headphones(c, sway, nod);

    // a note floats out of each cup in turn, up and away
    for (int k = 0; k < 2; k++) {
        uint32_t q = t + k * 1800 + (x.seed % 5) * 200;
        float p = (q % 3600) / 3600.0f;
        if (p > 0.75f) continue;
        uint32_t h = hash32(q / 3600 * 7 + k + x.seed);
        float s = k ? 1 : -1;
        float fx = 64 + s * (46 + 10 * p) - 4 + 2 * tsin(p * 2);
        float fy = 18 - 30 * p;
        c.glyph(iround(fx), iround(fy), (h >> 8) & 1 ? 0x266a : 0x266b, font_sym12);
    }
}

static const Anim A_WORDS = {"thinking_words", a_thinking_words, 4 * TW_WORD, LOOP};
static const Anim A_BIG_SPIN = {"big_spinner", a_big_spinner, 7040, LOOP};
static const Anim A_BULB = {"lightbulb", a_lightbulb, LB_CYC, LOOP};
static const Anim A_ORBIT = {"idea_orbit", a_idea_orbit, IO_CYC, LOOP};
static const Anim A_VIBE = {"vibe_think", a_vibe_think, 7200, LOOP};

ANIM_LIST(anims_think, &A_WORDS, &A_BIG_SPIN, &A_BULB, &A_ORBIT, &A_VIBE);
