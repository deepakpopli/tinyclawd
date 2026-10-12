// you type on the laptop: Clawd looks down at a little keyboard and watches
// the keys go down. x.typing is how fast you type (keys per second).
#include "anim.h"

static const uint32_t KB_IN = 350;   // eyes glide down, keyboard slides up
static const int KB_Y = 52;          // top edge of the keyboard
static const int NKEYS = 18;         // 11 on the top row, 3 + space + 3 below
static const int SPACE = 17;

// where key k sits, top-left and width
static void key_box(int k, int& x, int& y, int& w) {
    w = 7;
    if (k < 11) {
        x = 16 + k * 9;
        y = KB_Y + 2;
        return;
    }
    y = KB_Y + 7;
    if (k == SPACE) {
        x = 47;
        w = 33;
    } else {
        int i = k - 11;   // 0..5, three on each side of the space bar
        x = i < 3 ? 20 + i * 9 : 83 + (i - 3) * 9;
    }
}

// the keyboard, oy pushes it down (off screen when sliding), down = key held
static void keyboard(Canvas& c, int oy, int down) {
    if (oy >= 12) return;
    c.rect(10, KB_Y + oy, 117, 64 + oy);   // bottom edge hides below the screen
    for (int k = 0; k < NKEYS; k++) {
        int x, y, w;
        key_box(k, x, y, w);
        y += oy;
        if (k == down) c.fill_rect(x, y + 1, x + w - 1, y + 4);   // held: filled, one px lower
        else c.rect(x, y, x + w - 1, y + 3);
    }
}

// eyes looking down at the keys, k = 0 (straight ahead) .. 1 (down)
static Eye look_down(float k) {
    Eye e;
    e.dy = 9 * k;
    e.h = EYE_H - 6 * k;
    e.w = EYE_W + k;
    return e;
}

// which key the n-th press hits. the space bar a bit more often, like real typing
static int key_of(uint32_t n, uint32_t seed) {
    uint32_t h = hash32(n * 2654435761u + seed);
    return h % 7 == 0 ? SPACE : (int)((h >> 8) % (NKEYS - 1));
}

static float key_dx(int k) {
    int x, y, w;
    key_box(k, x, y, w);
    return (x + w / 2 - 64) / 11.0f;   // a few px left or right
}

static void a_you_typing(Canvas& c, uint32_t t, const Ctx& x) {
    float k = ease(seg(t, 0, KB_IN));
    int rate = x.typing < 1 ? 1 : (x.typing > 15 ? 15 : x.typing);
    bool fast = rate >= 8;
    Eye e = look_down(k);
    int down = -1;
    if (t >= KB_IN) {
        // one press every 1000 / rate ms
        uint32_t every = 1000 / rate, in = t - KB_IN;
        uint32_t n = in / every;
        float ph = (in % every) / (float)every;
        uint32_t hold = every * 6 / 10 < 160 ? every * 6 / 10 : 160;
        down = in % every < hold ? key_of(n, x.seed) : -1;
        // eyes jump to the key, fast typing is too quick to follow all the way
        float follow = fast ? 0.35f : 1;
        float from = n ? key_dx(key_of(n - 1, x.seed)) : 0;
        e.dx = follow * lerp(from, key_dx(key_of(n, x.seed)), ease(clamp01(ph * 3)));
        if (down >= 0 && !fast) e.dy += 1;   // a tiny nod on every key
        if (fast) {   // impressed: eyes open wide
            e.w += 2;
            e.h += 3;
        }
    }
    // a blink now and then
    uint32_t period = 3300 + x.seed % 900;
    if (t >= KB_IN + 500) face_blink(c, (t - KB_IN) % period, period - 300, e);
    else face(c, e);
    keyboard(c, iround(14 * (1 - k)), down);

    if (!fast || t < KB_IN) return;
    // speed lines flying out of both sides of the keyboard
    int off = (t / 40) % 6;
    for (int i = 0; i < 3; i++) {
        int y = KB_Y + 3 + i * 4;
        c.hline(6 - off - 3, 6 - off, y);
        c.hline(121 + off, 121 + off + 3, y);
    }
    if (rate >= 11) {   // really going: a sweat drop
        float bob = (t / 180) % 2;
        sweat(c, 108, 10 + bob);
    }
}

// a bold question mark, 7 x 12
static const uint8_t QMARK[] = {0x7c, 0xfe, 0xc6, 0x06, 0x0e, 0x1c, 0x38, 0x30, 0x30, 0x00, 0x30, 0x30};

// typing stopped: a look back up at you, "ready?"
static void a_typing_ready(Canvas& c, uint32_t t, const Ctx& x) {
    keyboard(c, iround(14 * ease(seg(t, 0, 300))), -1);
    const Key dy[] = {{0, 9}, {350, -5}, {850, -5}, {1150, 0}};
    const Key h[] = {{0, EYE_H - 6}, {350, EYE_H + 2}, {850, EYE_H + 2}, {1150, EYE_H}};
    const Key tilt[] = {{0, 0}, {400, 0}, {550, 1.5f}, {850, 1.5f}, {1100, 0}};
    float tl = KEYS(tilt, t) * ((x.seed & 1) ? 1 : -1);
    Eye l = eye(0, KEYS(dy, t) - tl, EYE_W, KEYS(h, t));
    Eye r = l;
    r.dy = KEYS(dy, t) + tl;
    r.mirror = true;
    face(c, l, r);
    if (t >= 380 && t < 1000) {
        int pop = iround(3 * (1 - ease_out(seg(t, 380, 500))));
        c.blit(104, 3 + pop, 8, 12, QMARK);
    }
}

// typing woke him up: eyes open softly, no grumpy face
static void a_typing_wake(Canvas& c, uint32_t t, const Ctx&) {
    const Key lid[] = {{0, 1}, {150, 1}, {330, 0.45f}, {450, 0.45f}, {620, 0}};
    Eye e;
    e.lid = KEYS(lid, t);
    face(c, e);
}

// shared by the typing loops below: x.typing as 1..15 keys per second
static int key_rate(const Ctx& x) {
    return x.typing < 1 ? 1 : (x.typing > 15 ? 15 : x.typing);
}

// typing_along: Clawd has his own tiny keyboard and types with you

static const int TA_X = 37, TA_Y = 10;   // mini Clawd at scale 1.5 (54 x 36)
static const int TA_KEYS = 22;           // 10 on the back row, 12 on the front row

static void ta_key(int k, int& x, int& y) {
    if (k < 10) {
        x = 29 + k * 7;
        y = 40;
    } else {
        x = 22 + (k - 10) * 7;
        y = 45;
    }
}

// press m: left claw on even presses, right claw on odd, each on its own side
static int ta_key_of(uint32_t m, uint32_t seed) {
    uint32_t h = hash32(m * 2654435761u + seed) >> 8;
    if (m & 1) return (h & 1) ? 7 + h % 3 : 18 + (h >> 2) % 4;
    return (h & 1) ? h % 3 : 10 + (h >> 2) % 4;
}

static void ta_keyboard(Canvas& c, int down) {
    const float pts[] = {26, 37, 101, 37, 111, 50, 16, 50};
    c.fill_poly(pts, 4, false);
    const float edge[] = {26, 37, 101, 37, 111, 50, 16, 50, 26, 37};
    c.polyline(edge, 5);
    for (int k = 0; k < TA_KEYS; k++) {
        int x, y;
        ta_key(k, x, y);
        if (k == down) c.fill_rect(x, y + 1, x + 4, y + 3);
        else c.rect(x, y, x + 4, y + 2);
    }
    c.hline(4, 123, 52);   // the desk
}

// a little claw, pincer up, cx = middle, y = bottom. a dark rim so it stands out
static void ta_claw(Canvas& c, int cx, int y) {
    c.fill_rect(cx - 4, y - 5, cx + 3, y + 1, false);
    c.fill_rect(cx - 3, y - 4, cx + 2, y);
    c.vline(cx, y - 4, y - 3, false);
}

static void a_typing_along(Canvas& c, uint32_t t, const Ctx& x) {
    int rate = key_rate(x);
    uint32_t every = 1000 / rate;
    uint32_t n = t / every + 2;   // +2 so n - 2 is never negative
    uint32_t in = t % every;
    uint32_t hold = every / 2 < 120 ? every / 2 : 120;
    bool down = in < hold;

    // proud bob: a dip on every key and a slow sway
    int y = TA_Y + (down ? 1 : 0) - (int)breathe(t, 1700);
    uint32_t period = 3100 + x.seed % 900;
    Eyes eyes = (t % period) > period - 140 ? EYES_CLOSED : EYES_OPEN;
    clawd(c, TA_X, y, pose(eyes, ARMS_DOWN, FEET_NONE), 1.5f);
    ta_keyboard(c, down ? ta_key_of(n, x.seed) : -1);

    // each claw: down on its key, then hops over to its next one
    for (int side = 0; side < 2; side++) {
        uint32_t m = ((n & 1) == (uint32_t)side) ? n : n - 1;
        uint32_t tt = t - (m - 2) * every;
        int kx, ky, nx, ny;
        ta_key(ta_key_of(m, x.seed), kx, ky);
        ta_key(ta_key_of(m + 2, x.seed), nx, ny);
        float cx = kx + 2, cy = ky;
        if (tt >= hold) {
            float p = clamp01((tt - hold) / (float)(2 * every - hold));
            float e = ease(p);
            cx = lerp(kx + 2, nx + 2, e);
            cy = lerp(ky, ny, e) - 3 - 4 * p * (1 - p) * 4;
        }
        int sx = side ? TA_X + 51 : TA_X + 4, sy = y + 15;
        c.line(sx, sy, cx, cy - 4, 2);
        ta_claw(c, iround(cx), iround(cy));
    }

    // every 16 keys a proud little sparkle
    uint32_t since = (n % 16) * every + in;
    if (since < 350) {
        int r = 1 + since / 90;
        sparkle(c, 24, 20, r);
        sparkle(c, 106, 24, r);
    }
    if (rate >= 11) {   // really fast: he is working hard to keep up
        sweat(c, 96, 1 + (t / 200) % 2);
        int off = (t / 50) % 3;
        c.hline(8 - off, 12 - off, 41);
        c.hline(6 - off, 10 - off, 46);
        c.hline(115 + off, 119 + off, 41);
        c.hline(117 + off, 121 + off, 46);
    }
}

static const Anim A_YOU_TYPING = {"you_typing", a_you_typing, 4000, LOOP | CALM_IN};
static const Anim A_TYPING_READY = {"typing_ready", a_typing_ready, 1200, CALM_OUT};
static const Anim A_TYPING_WAKE = {"typing_wake", a_typing_wake, 700, CALM_OUT};
static const Anim A_TYPING_ALONG = {"typing_along", a_typing_along, 4000, LOOP | SCENE};

ANIM_LIST(anims_keys, &A_YOU_TYPING, &A_TYPING_READY, &A_TYPING_WAKE, &A_TYPING_ALONG);
