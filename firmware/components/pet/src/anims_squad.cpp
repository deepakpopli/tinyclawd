// subagents: a half size Clawd for each one
#include <stdio.h>

#include "anim.h"

static const int MINI_W = 18, MINI_H = 12;

static int squad_size(const Ctx& x) {
    int n = x.subagents;
    return n < 1 ? 1 : (n > 8 ? 8 : n);
}

// top left of mini i out of n: one centred row up to 4, two rows above that
static void slot(int i, int n, float& px, float& py) {
    const int GAP = 27;
    int top = n <= 4 ? n : (n + 1) / 2;
    int row = i < top ? 0 : 1;
    int in_row = row ? n - top : top;
    int k = row ? i - top : i;
    float w = (in_row - 1) * GAP + MINI_W;
    px = 64 - w / 2 + k * GAP;
    py = n <= 4 ? 24 : (row ? 38 : 10);
}

// eyes towards something at ax
static Eyes look_at(float x, float ax) {
    float cx = x + MINI_W / 2.0f;
    return ax < cx - 4 ? EYES_LEFT : (ax > cx + 4 ? EYES_RIGHT : EYES_OPEN);
}

// ---------------------------------------------------------------- subagents
// they pop in one by one, then pass the ball around. loops

// random order of the squad for round r, everyone gets the ball once a round
static void shuffled(int* o, int n, uint32_t r, uint32_t seed) {
    for (int i = 0; i < n; i++) o[i] = i;
    for (int i = n - 1; i > 0; i--) {
        int j = hash32(r * 7919 + i * 31 + seed) % (i + 1);
        int tmp = o[i];
        o[i] = o[j];
        o[j] = tmp;
    }
}

// who has the ball at pass k. never passes to itself
static int holder(uint32_t k, int n, uint32_t seed) {
    if (n == 1) return 0;
    if (n == 2) return k % 2;
    uint32_t r = k / n;
    int o[8], prev[8];
    shuffled(o, n, r, seed);
    if (r > 0) {
        shuffled(prev, n, r - 1, seed);
        if (o[0] == prev[n - 1]) {   // swap the first two, the last one stays put
            int tmp = o[0];
            o[0] = o[1];
            o[1] = tmp;
        }
    }
    return o[k % n];
}

static void a_subagents(Canvas& c, uint32_t t, const Ctx& x) {
    int n = squad_size(x);
    const uint32_t POP = 180, PASS = 450;
    uint32_t popped = n * POP + 200;
    char s[4];
    snprintf(s, sizeof(s), "x%d", n);
    c.text(127 - c.text_width(s, font_5x7), 56, s, font_5x7);

    if (t < popped) {
        for (int j = 0; j < n; j++) {
            uint32_t at = j * POP;
            if (t < at) break;
            float px, py;
            slot(j, n, px, py);
            int pop = t - at < 70 ? 2 : 0;
            clawd(c, iround(px), iround(py) - pop, pose(), 0.5f);
        }
        return;
    }

    uint32_t k = (t - popped) / PASS;
    float u = ((t - popped) % PASS) / (float)PASS;
    int from = holder(k, n, x.seed), to = holder(k + 1, n, x.seed);
    float x0, y0, x1, y1;
    slot(from, n, x0, y0);
    slot(to, n, x1, y1);
    float bx, by;
    if (n == 1) {                            // alone: keepy uppy
        bx = x0 + 15;
        by = y0 + 11 - 14 * sinf(u * (float)M_PI);
    } else {
        bx = lerp(x0 + 9, x1 + 9, u);
        by = lerp(y0 + 11, y1 + 11, u) - 10 * sinf(u * (float)M_PI);
    }
    for (int j = 0; j < n; j++) {
        float px, py;
        slot(j, n, px, py);
        int hop = iround(1.5f * (1 + sinf(u * 2 * (float)M_PI + j)));
        bool kicker = j == from && u < 0.2f;
        clawd(c, iround(px), iround(py) - hop, pose(look_at(px, bx), ARMS_DOWN, kicker ? FEET_LEFT : FEET_BOTH), 0.5f);
    }
    ball(c, bx, by, 2);
}

static const Anim A_SUBAGENTS = {"subagents", a_subagents, 6000, LOOP | SCENE};

ANIM_LIST(anims_squad, &A_SUBAGENTS);
