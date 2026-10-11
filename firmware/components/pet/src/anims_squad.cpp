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

// ---------------------------------------------------------- subagents march
// the squad walks out across the screen like a team coming out of the tunnel

static void a_subagents_march(Canvas& c, uint32_t t, const Ctx& x) {
    int n = squad_size(x);
    uint32_t h = hash32(x.seed + 3);
    bool back = h & 1;                       // walks right to left
    bool ball_ = h & 2;                      // the first one brings the ball
    const float T = 3000;
    float v = (150 + (n - 1) * 24) / T;
    c.hline(0, 127, 63);
    for (int j = 0; j < n; j++) {
        float px = -22 - j * 24 + v * t;
        if (px < -20 || px > 128) continue;
        int step = (t / 120 + j) % 2;
        int sx = back ? 128 - MINI_W - iround(px) : iround(px);
        clawd(c, sx, 50 - step, pose(EYES_OPEN, ARMS_DOWN, step ? FEET_LEFT : FEET_RIGHT), 0.5f);
        if (j == 0 && ball_) {
            float bx = px + 21 + 1.5f * sinf(t / 60.0f);
            ball(c, back ? 127 - bx : bx, 60, 2);
        }
    }
    char s[16];
    snprintf(s, sizeof(s), n == 1 ? "%d subagent" : "%d subagents", n);
    if (back) c.text(126 - c.text_width(s, font_5x7), 2, s, font_5x7);
    else c.text(2, 2, s, font_5x7);
}

// ------------------------------------------------------------ squad return
// the subagents are done: they run back in and merge into one Clawd

static void a_squad_return(Canvas& c, uint32_t t, const Ctx& x) {
    int n = x.subagents < 2 ? 4 : (x.subagents > 8 ? 8 : x.subagents);
    const uint32_t RUN = 900, MERGE = 1400, GROW = 1650, END = 3000;
    const float CX = 64 - MINI_W / 2.0f, CY = 26;

    if (t < MERGE) {
        for (int i = 0; i < n; i++) {
            uint32_t at = i * 70;
            if (t < at) continue;
            float k = seg(t, at, at + RUN);
            bool right = (i + x.seed) % 2;
            float lane = 6 + (n > 1 ? i * 40.0f / (n - 1) : 20);
            float sx = right ? 124 : 4 - MINI_W;
            float px = lerp(sx, CX, ease(k)), py = lerp(lane, CY, ease(k));
            int step = (t / 90 + i) % 2;
            Pose p = pose(right ? EYES_LEFT : EYES_RIGHT, ARMS_DOWN, k < 1 ? (step ? FEET_LEFT : FEET_RIGHT) : FEET_BOTH);
            if (k >= 1) p.eyes = EYES_OPEN;
            clawd(c, iround(px), iround(py) - (k < 1 ? step : 0), p, 0.5f);
        }
        return;
    }
    if (t < GROW) {
        // poof: ring going out, the mini grows to full size
        float k = seg(t, MERGE, GROW);
        float r = 4 + 26 * ease_out(k);
        if (k < 0.9f) c.ellipse(64 - r, 32 - r * 0.6f, 64 + r, 32 + r * 0.6f);
        if (k < 0.4f) clawd(c, iround(CX), iround(CY) + (k < 0.2f ? 1 : -2), pose(EYES_CLOSED), 0.5f);
        else clawd(c, 46, 20 - iround(3 * (1 - k)), pose(EYES_CLOSED));
        return;
    }
    uint32_t u = t - GROW;
    static const float J[] = {0, 3, 5, 3, 0};
    bool rest = t > END - 350;
    float j = rest ? 0 : pickf(J, 5, u / 90.0f);
    clawd(c, 46, 20 - iround(j), pose(u > 500 && u < 800 ? EYES_WINK : EYES_OPEN, rest ? ARMS_DOWN : ARMS_UP));
    if (!rest) {
        static const int A[][2] = {{24, 14}, {104, 18}, {20, 44}, {108, 40}};
        static const int B[][2] = {{30, 26}, {98, 10}, {26, 54}, {102, 50}};
        const int (*p)[2] = (u / 120) % 2 ? A : B;
        for (int k = 0; k < 4; k++) sparkle(c, p[k][0], p[k][1], 2);
    }
}

static const Anim A_SUBAGENTS = {"subagents", a_subagents, 6000, LOOP | SCENE};
static const Anim A_MARCH = {"subagents_march", a_subagents_march, 3000, SCENE};
static const Anim A_RETURN = {"squad_return", a_squad_return, 3000, SCENE};

ANIM_LIST(anims_squad, &A_SUBAGENTS, &A_MARCH, &A_RETURN);
