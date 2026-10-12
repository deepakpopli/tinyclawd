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


static float blink_k(uint32_t t, uint32_t at) {
    return t >= at && t < at + BLINK_MS ? (t - at) / (float)BLINK_MS : -1;
}

static Eye maybe_blink(const Eye& e, float k) { return k >= 0 ? blink_eye(e, k) : e; }

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

static const Anim A_WORDS = {"thinking_words", a_thinking_words, 4 * TW_WORD, LOOP};

ANIM_LIST(anims_think, &A_WORDS);
