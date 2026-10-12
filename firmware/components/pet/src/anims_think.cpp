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

static const Anim A_WORDS = {"thinking_words", a_thinking_words, 4 * TW_WORD, LOOP};
static const Anim A_BIG_SPIN = {"big_spinner", a_big_spinner, 7040, LOOP};

ANIM_LIST(anims_think, &A_WORDS, &A_BIG_SPIN);
