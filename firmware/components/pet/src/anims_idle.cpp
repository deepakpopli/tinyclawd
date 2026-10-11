// idle life: dozing off, sleeping, dreaming, waking up, music and small moods
#include "anim.h"

static const float PI = (float)M_PI;

// ------------------------------------------------------------------- helpers

// closed eyes breathing slowly, with a little shuffle every ~8 s
static void sleep_face(Canvas& c, uint32_t t, uint32_t seed, float look = 0) {
    float br = 0.5f - 0.5f * cosf(2 * PI * (t % 3600) / 3600.0f);
    Eye e;
    e.lid = 1;
    e.dy = br * 1.5f;
    e.w = EYE_W + br;
    e.dx = look;
    uint32_t slot = t / 8000, in = t % 8000;
    uint32_t h = hash32(slot * 31 + seed);
    uint32_t at = 2000 + h % 4000;
    if (in >= at && in < at + 900) {
        float k = sinf(PI * seg(in, at, at + 900));
        e.dx += ((h >> 12) & 1 ? 2.5f : -2.5f) * k;
        e.dy -= 1.5f * k;
        e.w += 2 * k;
    }
    face(c, e);
}

// the cross shaped vein from the angry face
static void vein(Canvas& c, int x, int y) {
    c.line(x, y, x + 4, y + 4);
    c.line(x + 8, y, x + 4, y + 4);
    c.line(x, y + 10, x + 4, y + 6);
    c.line(x + 8, y + 10, x + 4, y + 6);
}

static void note(Canvas& c, float x, float y, uint32_t h) {
    c.glyph(iround(x), iround(y), h & 1 ? 0x266a : 0x266b, font_sym12);
}

// -------------------------------------------------------------------- sleepy

static void a_sleepy(Canvas& c, uint32_t t, const Ctx&) {
    const Key lid[] = {{0, 0}, {600, 0}, {1160, 0.45f}, {1760, 0.45f}, {2120, 0.9f},
                       {2480, 0.5f}, {2980, 0.5f}, {3620, 1}};
    const Key nod[] = {{0, 0}, {1760, 0}, {2120, 2}, {2480, 0}, {2980, 0}, {3620, 1}, {4200, 0}};
    Eye e;
    e.lid = KEYS(lid, t);
    e.dy = KEYS(nod, t);
    face(c, e);
    if (t >= 3620) zzz(c, (t - 3620) / 1330.0f);
}

// ------------------------------------------------------------------- wake up

static void a_wake_up(Canvas& c, uint32_t t, const Ctx&) {
    const Key lid[] = {{0, 1}, {700, 1}, {1000, 0.6f}, {1300, 0.6f}, {1435, 1}, {1705, 0}};
    const Key grow[] = {{0, 0}, {2070, 0}, {2270, 1}, {2670, 1}, {2870, 0}};
    Eye e;
    e.lid = KEYS(lid, t);
    float g = KEYS(grow, t);
    e.w = EYE_W + 4 * g;
    e.h = EYE_H + 5 * g;
    if (t >= 1705 && t < 1705 + BLINK_MS) e = blink_eye(Eye(), (t - 1705) / (float)BLINK_MS);
    face(c, e);
}

// ---------------------------------------------------------------------- peek

static void a_peek(Canvas& c, uint32_t t, const Ctx& x) {
    float s = (x.seed & 2) ? 1 : -1;
    const Key dy[] = {{0, 0}, {400, 0}, {520, -3}, {820, 48}, {1400, 48}, {1700, 18},
                      {3000, 18}, {3260, 0}};
    const Key dx[] = {{0, 0}, {1750, 0}, {1900, -10 * s}, {2300, -10 * s}, {2480, 10 * s},
                      {2850, 10 * s}, {3000, 0}};
    Eye e;
    e.dy = KEYS(dy, t);
    e.dx = KEYS(dx, t);
    // stretch while popping up, squash on the duck
    if (t >= 1400 && t < 1800) e.h = EYE_H + 3 * sinf(PI * seg(t, 1400, 1800));
    if (t >= 520 && t < 820) e.w = EYE_W + 2 * sinf(PI * seg(t, 520, 820));
    // a curious little blink while half way up
    if (t >= 2550 && t < 2550 + BLINK_MS) e = blink_eye(e, (t - 2550) / (float)BLINK_MS);
    face_blink(c, t, 3400, e);
}

// --------------------------------------------------------------------- music

static void a_music(Canvas& c, uint32_t t, const Ctx& x) {
    float amp = 0.75f + 0.25f * sinf(2 * PI * t / 7300.0f);
    float sway = 5 * amp * sinf(2 * PI * t / 1120.0f);
    float bob = 1.5f * fabsf(sinf(2 * PI * t / 560.0f));
    // every few bars it really gets into it
    uint32_t bar = t / 4480, hb = hash32(bar * 13 + x.seed);
    if (hb % 3 == 0) bob *= 2;
    Eye e;
    e.shape = HAPPY;
    e.dx = sway;
    e.dy = bob + 2;
    face(c, e);
    for (int k = 0; k < 2; k++) {
        float q = t / 1120.0f + k * 0.5f;
        uint32_t n = (uint32_t)q;
        float p = q - n;
        uint32_t h = hash32(n * 7 + k + x.seed);
        float base = (h >> 4) % 3 ? 100 : 14;
        note(c, base + 8 * sinf(p * 6 + k), 46 - 46 * p, h >> 9);
    }
}

// ---------------------------------------------------------------------- yawn

static void a_yawn(Canvas& c, uint32_t t, const Ctx& x) {
    const Key st[] = {{0, 0}, {300, 0}, {1100, 1}, {1300, 1}, {1450, 0}};
    float k = KEYS(st, t);
    Eye e;
    e.h = EYE_H + 6 * k;
    e.w = EYE_W - 2 * k;
    e.dy = -3 * k + 0.6f * sinf(t / 40.0f) * (t > 1100 && t < 1300);
    if (t < 1300) {
        face(c, e);
        return;
    }
    if (t < 1450) {   // squeezing shut
        e.lid = seg(t, 1300, 1450);
        face(c, e);
        return;
    }
    if (t < 2550) {   // the big yawn, eyes squeezed into curves
        Eye h;
        h.shape = HAPPY;
        h.dy = 2 + 0.8f * sinf(t / 55.0f);
        h.dx = 0.7f * sinf(t / 31.0f);
        face(c, h);
        if (t > 2000 && (x.seed & 1)) {   // a tiny tear sometimes
            float d = 4 * seg(t, 2000, 2550);
            tear(c, LEFT_X - 15, 38 + d);
            tear(c, RIGHT_X + 15, 38 + d);
        }
        return;
    }
    const Key lid[] = {{2550, 1}, {2900, 0.5f}, {3150, 0.5f}, {3300, 0.6f}, {3500, 0}};
    Eye o;
    o.lid = KEYS(lid, t);
    o.dy = 1 - seg(t, 2550, 3500);
    face(c, o);
}

// -------------------------------------------------------------------- asleep

static void a_asleep(Canvas& c, uint32_t t, const Ctx& x) {
    sleep_face(c, t, x.seed);
    zzz(c, t / 3000.0f);
    // now and then a sleep bubble grows with the breathing and pops
    uint32_t slot = t / 11000, in = t % 11000;
    uint32_t h = hash32(slot * 977 + x.seed);
    if (slot == 0 || h % 3) return;
    uint32_t a = 1800 + h % 1800, b = a + 5400;
    if (in < a || in >= b + 200) return;
    float br = 0.5f - 0.5f * cosf(2 * PI * (t % 3600) / 3600.0f);
    float bx = 64, by = 47;
    if (in < b) {
        float r = 1.5f + seg(in, a, b) * 3 + br * 3;
        c.ellipse(bx - r, by - r, bx + r, by + r);
    } else {
        for (int i = 0; i < 6; i++) {
            float an = i * PI / 3, r = 6 + 4 * seg(in, b, b + 200);
            c.px(iround(bx + r * cosf(an)), iround(by + r * sinf(an)));
        }
    }
}

// --------------------------------------------------------------------- dream

// cloud made of circles: filled, then hollowed out by smaller ones
static const float CLOUD[][3] = {{-22, 3, 8}, {-10, -4, 10}, {4, -5, 11}, {18, -2, 9},
                                 {25, 5, 7}, {10, 6, 9}, {-7, 6, 9}, {-24, 7, 6}};

static void cloud(Canvas& c, float cx, float cy, float k) {
    for (auto& p : CLOUD) c.fill_circle(cx + p[0] * k, cy + p[1] * k, p[2] * k + 1);
    for (auto& p : CLOUD) c.fill_circle(cx + p[0] * k, cy + p[1] * k, p[2] * k - 0.5f, false);
}

static void bubble(Canvas& c, float x, float y, float r) {
    c.fill_circle(x, y, r + 0.8f);
    if (r > 1.5f) c.fill_circle(x, y, r - 0.6f, false);
}

static const float CX = 89, CY = 18;

// what happens inside the cloud, t from 0, returns the ball x for the sleeper's eyes
static float dream_inside(Canvas& c, uint32_t t, uint32_t seed) {
    const int sx = 71, sy = 8;
    if (seed & 4) {
        // keepy uppy, then a big kick and arms up
        uint32_t hop = t / 560, in = t % 560;
        if (hop < 7) {
            bool right = hop & 1;
            float f0 = right ? sx + 32 : sx + 4, f1 = right ? sx + 4 : sx + 32;
            float p = in / 560.0f;
            float bx = lerp(f0, f1, p), by = 28 - 23 * sinf(PI * p);
            Feet ft = in < 110 ? (right ? FEET_LEFT : FEET_RIGHT) : FEET_BOTH;
            clawd(c, sx, sy, pose(EYES_OPEN, ARMS_DOWN, ft));
            ball(c, bx, by, 2);
            return bx;
        }
        float p = clamp01((t - 7 * 560) / 600.0f);
        clawd(c, sx, sy, pose(p > 0.3f ? EYES_CLOSED : EYES_OPEN, p > 0.2f ? ARMS_UP : ARMS_DOWN));
        if (p < 1) ball(c, sx + 32 + 14 * p, 28 - 26 * p, 2);
        return sx + 18;
    }
    // a ball bouncing around, mini Clawd watching it, then a happy hop
    float bx = 89 + 21 * sinf(t / 520.0f);
    float by = 29 - 22 * fabsf(sinf(t / 300.0f));
    Eyes look = bx < sx + 12 ? EYES_LEFT : (bx > sx + 24 ? EYES_RIGHT : EYES_OPEN);
    bool hop = t > 3600 && t < 4300;
    clawd(c, sx, sy - (hop ? iround(3 * sinf(PI * seg(t, 3600, 4300))) : 0),
          pose(hop ? EYES_CLOSED : look, hop ? ARMS_UP : ARMS_DOWN));
    ball(c, bx, by, 2);
    return bx;
}

static void a_dream(Canvas& c, uint32_t t, const Ctx& x) {
    const uint32_t D0 = 2300, D1 = 6900;   // dream content window
    float look = 0;
    bool b1 = t >= 1300 && t < 7600, b2 = t >= 1650 && t < 7350;
    const Key grow[] = {{2000, 0.05f}, {2250, 1.14f}, {2400, 1.05f}};
    float k = t >= 2000 && t < 7000 ? KEYS(grow, t) : 0;
    if (b1) bubble(c, 61, 45, 1.2f);
    if (b2) bubble(c, 54, 38, 2.3f);
    if (k > 0) {
        cloud(c, CX, CY, k);
        if (t >= D0 && t < D1) look = (dream_inside(c, t - D0, x.seed) - 89) / 14;
    }
    if (t >= 7000 && t < 7300) {   // pop
        float p = seg(t, 7000, 7300);
        for (int i = 0; i < 8; i++) {
            float a = i * PI / 4;
            float r0 = 10 + 14 * p, r1 = r0 + 4;
            c.line(CX + r0 * cosf(a), CY + r0 * sinf(a) * 0.6f, CX + r1 * cosf(a), CY + r1 * sinf(a) * 0.6f);
        }
    }
    sleep_face(c, t, x.seed, clampf(look, -1.5f, 1.5f));
    if (t < 1300 || t >= 7900) zzz(c, t / 3000.0f);
}

// --------------------------------------------------------------------- bored

static void a_bored(Canvas& c, uint32_t t, const Ctx& x) {
    float s = (x.seed & 8) ? 1 : -1;
    const Key lid[] = {{0, 0}, {300, 0}, {700, 0.45f}, {2700, 0.45f}, {3200, 0.62f},
                       {3900, 0.45f}, {4700, 0.45f}, {5000, 0}};
    const Key dx[] = {{0, 0}, {900, 0}, {1800, 12 * s}, {4200, 12 * s}, {4800, 0}};
    const Key dy[] = {{0, 0}, {2700, 0}, {3200, 3}, {3900, 0}};
    Eye e;
    e.lid = KEYS(lid, t);
    e.dx = KEYS(dx, t);
    e.dy = KEYS(dy, t) + (t < 2700 ? breathe(t, 2000) : 0);
    face_blink(c, t, 5050, e);
    if (t >= 1900 && t < 4200) {
        int n = t < 2200 ? 1 : (t < 2500 ? 2 : 3);
        int x0 = s > 0 ? 106 : 12;
        for (int i = 0; i < n; i++) c.fill_rect(x0 + i * 4, 12, x0 + i * 4 + 1, 13);
    }
}

// ------------------------------------------------------------------ side eye

static void a_side_eye(Canvas& c, uint32_t t, const Ctx& x) {
    float s = (x.seed & 16) ? 1 : -1;
    const Key lid[] = {{0, 0}, {300, 0}, {550, 0.4f}, {1500, 0.4f}, {1650, 0.55f},
                       {1900, 0.4f}, {3050, 0.4f}, {3300, 0}};
    const Key dx[] = {{0, 0}, {350, 0}, {800, 13 * s}, {2550, 13 * s}, {3000, 0}};
    Eye e;
    e.lid = KEYS(lid, t);
    e.dx = KEYS(dx, t);
    e.dy = 0.4f * e.lid * 2.5f * (t < 3300);
    // a quick suspicious double check to the other side sometimes
    if ((x.seed & 32) && t >= 2050 && t < 2450) e.dx -= 7 * s * sinf(PI * seg(t, 2050, 2450));
    face_blink(c, t, 3350, e);
}

// --------------------------------------------------------------------- dance

static void a_dance(Canvas& c, uint32_t t, const Ctx& x) {
    const uint32_t T0 = 300, BEAT = 500, NB = 8;
    if (t < T0 || t >= T0 + BEAT * NB) {
        face(c);
        return;
    }
    uint32_t b = (t - T0) / BEAT;
    float p = ((t - T0) % BEAT) / (float)BEAT;
    float env = b == 0 ? ease(p * 2) : (b == NB - 1 ? 1 - ease(p) : 1);
    float side = ((b + (x.seed >> 6)) & 1) ? 1 : -1;
    float up = sinf(PI * p) * env;
    Eye l, r;
    r.mirror = true;
    l.dy = r.dy = -5 * up;
    l.dx = r.dx = side * 4 * up;
    l.dy += side * 2 * up;
    r.dy -= side * 2 * up;
    if (p < 0.15f && b > 0) {   // squash on landing
        float q = 1 - p / 0.15f;
        l.w = r.w = EYE_W + 2 * q;
        l.h = r.h = EYE_H - 3 * q;
        l.dy += 1.5f * q;
        r.dy += 1.5f * q;
    }
    face(c, l, r);
    // a note pops on each beat, on alternating sides
    for (uint32_t k = 0; k < 2; k++) {
        if (b < k) break;
        uint32_t bb = b - k;
        float q = (((t - T0) - bb * BEAT)) / 900.0f;
        if (q >= 1 || bb >= NB - 1) continue;
        float nx = ((bb + (x.seed >> 6)) & 1) ? 108 : 10;
        note(c, nx + 3 * sinf(q * 5), 40 - 34 * q, hash32(bb + x.seed));
    }
}

// --------------------------------------------------------------- grumpy wake

static void a_grumpy_wake(Canvas& c, uint32_t t, const Ctx& x) {
    bool right = x.seed & 1;
    if (t < 400) {
        sleep_face(c, 0, x.seed);
        return;
    }
    if (t >= 2000) {
        float k = 0.5f + 0.5f * seg(t, 2000, 2120);
        Eye e = t < 2120 ? blink_eye(Eye(), k) : Eye();
        e.dy = 0;
        face_blink(c, t, 2750, e);
        return;
    }
    float sh = 0;
    if (t >= 1150 && t < 1650) sh = 1.6f * sinf(t / 14.0f) * sinf(PI * seg(t, 1150, 1650));
    Eye shut;
    shut.lid = 1;
    shut.dx = sh;
    Eye open = shut;
    if (t < 700) open.lid = lerp(1, 0.5f, ease(seg(t, 400, 700)));
    else if (t < 1750) {
        open.shape = ANGRY;
        open.h = 14;
        open.dy = 4;
        open.lid = 0;
    } else open.lid = lerp(0.5f, 1, ease(seg(t, 1750, 1950)));
    if (right) {
        open.mirror = true;
        face(c, shut, open);
    } else {
        shut.mirror = true;
        face(c, open, shut);
    }
    if (t >= 800 && t < 1750) vein(c, right ? 104 : 16, 6);
}

static const Anim A_SLEEPY = {"sleepy", a_sleepy, 6300, CALM_IN};
static const Anim A_WAKE = {"wake_up", a_wake_up, 3470, CALM_OUT};
static const Anim A_PEEK = {"peek", a_peek, 3900, CALM};
static const Anim A_MUSIC = {"music", a_music, 8960, LOOP};
static const Anim A_YAWN = {"yawn", a_yawn, 3800, CALM};
static const Anim A_ASLEEP = {"asleep", a_asleep, 9000, LOOP};
static const Anim A_DREAM = {"dream", a_dream, 9600, 0};
static const Anim A_BORED = {"bored", a_bored, 5600, CALM};
static const Anim A_SIDE = {"side_eye", a_side_eye, 3800, CALM};
static const Anim A_DANCE = {"dance", a_dance, 4600, CALM};
static const Anim A_GRUMPY = {"grumpy_wake", a_grumpy_wake, 3200, CALM_OUT};

ANIM_LIST(anims_idle, &A_SLEEPY, &A_WAKE, &A_PEEK, &A_MUSIC, &A_YAWN, &A_ASLEEP, &A_DREAM,
          &A_BORED, &A_SIDE, &A_DANCE, &A_GRUMPY);
