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

// letters_fly: each key you press flies up into a thought bubble

static const uint32_t LF_IN = 400;     // eyes move aside, keyboard and bubble come in
static const uint32_t LF_FLY = 520;    // one letter's flight
static const int LF_KEYS = 12;
static const int LF_COLS = 7, LF_ROWS = 3, LF_CAP = LF_COLS * LF_ROWS;
static const int LF_BX = 81, LF_BY = 5;   // first letter slot in the bubble

static int lf_key_of(uint32_t m, uint32_t seed) {
    return hash32(m * 2246822519u + seed) % LF_KEYS;
}

// what you seem to be typing, a new mix every bubble
static const char* const LF_WORDS[] = {"fix", "the", "bug", "add", "a", "test", "make", "it", "work",
                                       "pls", "hi", "clawd", "code", "now", "ok", "ship", "tiny", "why"};
static const int LF_NWORDS = sizeof(LF_WORDS) / sizeof(LF_WORDS[0]);

// the letter press m types, 0 = a space. words never break over two lines
static char lf_char(uint32_t m, uint32_t seed) {
    uint32_t first = m - m % LF_CAP;
    int want = m % LF_CAP, pos = 0;
    for (uint32_t w = 0;; w++) {
        const char* s = LF_WORDS[hash32(first * 31 + w * 7919 + seed) % LF_NWORDS];
        int len = strlen(s);
        int col = pos % LF_COLS;
        if (col + len > LF_COLS) pos += LF_COLS - col;
        if (want < pos) return 0;
        if (want < pos + len) return s[want - pos];
        pos += len + 1;
        if (want < pos) return 0;
    }
}

static void lf_slot(uint32_t m, int& x, int& y) {
    int i = m % LF_CAP;
    x = LF_BX + (i % LF_COLS) * 6;
    y = LF_BY + (i / LF_COLS) * 9;
}

// round box with a 1 px border, the inside cleared
static void lf_bubble(Canvas& c, int x0, int y0, int x1, int y1) {
    c.fill_rect(x0 + 2, y0, x1 - 2, y1);
    c.fill_rect(x0, y0 + 2, x1, y1 - 2);
    c.px(x0 + 1, y0 + 1);
    c.px(x1 - 1, y0 + 1);
    c.px(x0 + 1, y1 - 1);
    c.px(x1 - 1, y1 - 1);
    c.fill_rect(x0 + 2, y0 + 1, x1 - 2, y1 - 1, false);
    c.fill_rect(x0 + 1, y0 + 2, x1 - 1, y1 - 2, false);
}

static void a_letters_fly(Canvas& c, uint32_t t, const Ctx& x) {
    int rate = key_rate(x);
    float k = ease(seg(t, 0, LF_IN));
    uint32_t every = 1000 / rate;
    bool started = t >= LF_IN;
    uint32_t in = started ? t - LF_IN : 0;
    uint32_t n = in / every;   // last press so far
    int landed = in >= LF_FLY ? (int)((in - LF_FLY) / every) : -1;   // last letter in the bubble

    // the bubble with its trail of dots
    int pop = iround(4 * (1 - k));
    if (k > 0.3f) {
        lf_bubble(c, 78 + pop, 2 + pop, 124 - pop, 31 - pop);
        c.fill_circle(73, 38, 1.5f);
        c.fill_circle(77, 34, 2);
    }
    if (landed >= 0) {
        uint32_t first = landed - landed % LF_CAP;   // the bubble empties when full
        for (uint32_t m = first; m <= (uint32_t)landed; m++) {
            char ch = lf_char(m, x.seed);
            if (!ch) continue;
            int sx, sy;
            lf_slot(m, sx, sy);
            char s[2] = {ch, 0};
            c.text(sx, sy, s);
        }
        // just emptied: a sparkle where the words went
        uint32_t since = in - LF_FLY - first * every;
        if (first > 0 && since < 300) sparkle(c, 101, 17, 2 + since / 60);
    }
    // a cursor waits in the next slot
    if (started && (t / 300) % 2) {
        int sx, sy;
        lf_slot(landed + 1, sx, sy);
        c.hline(sx, sx + 4, sy + 7);
    }

    // the keyboard: one row of keys along the bottom
    int ky = 55 + iround(10 * (1 - k));
    int down = (started && in % every < 110) ? lf_key_of(n, x.seed) : -1;
    c.hline(0, 127, ky);
    for (int i = 0; i < LF_KEYS; i++) {
        int kx = 5 + i * 10;
        if (i == down) c.fill_rect(kx, ky + 4, kx + 7, ky + 8);
        else c.rect(kx, ky + 3, kx + 7, ky + 7);
    }

    // flying letters, an arc up from the key to the slot. the eyes follow them
    float gx = 0, gy = 0;
    int flying = 0;
    uint32_t oldest = landed + 1;
    for (uint32_t m = oldest; started && m <= n; m++) {
        float p = (in - m * every) / (float)LF_FLY;
        if (p >= 1) continue;
        float sx0 = 9 + lf_key_of(m, x.seed) * 10, sy0 = ky - 6;
        int ex, ey;
        lf_slot(m, ex, ey);
        // quadratic curve: shoot up first, then swing right into the bubble
        float e = ease_out(p), u = 1 - e;
        float px = u * u * sx0 + 2 * u * e * sx0 + e * e * ex;
        float py = u * u * sy0 + 2 * u * e * (ey - 6) + e * e * ey;
        gx += px;
        gy += py;
        flying++;
        char ch = lf_char(m, x.seed);
        int ix = iround(px), iy = iround(py);
        if (p < 0.7f) c.fill_rect(ix - 1, iy - 1, ix + 5, iy + 7, false);   // a dark rim until it is in
        if (ch) {
            char s[2] = {ch, 0};
            c.text(ix, iy, s);
        } else {
            c.fill_rect(ix + 1, iy + 3, ix + 3, iy + 4);   // a space: a little block
        }
    }

    // eyes: smaller and off to the left, watching the letters
    if (flying) {
        gx /= flying;
        gy /= flying;
    } else {
        int sx, sy;
        lf_slot(landed < 0 ? 0 : landed, sx, sy);
        gx = sx;
        gy = sy;
    }
    float lx = started ? clampf((gx - 50) / 9, -3, 4) : 0;
    float ly = started ? clampf((gy - 30) / 5, -4, 4) : 0;
    Eye l = eye(-18 * k + lx, -2 * k + ly, EYE_W - 2 * k, EYE_H - 6 * k);
    if (rate >= 11 && started) {   // wow, so many letters
        l.w += 2;
        l.h += 2;
    }
    uint32_t period = 2900 + x.seed % 1100;
    if (started && in % period > period - BLINK_MS)
        l = blink_eye(l, (in % period - (period - BLINK_MS)) / (float)BLINK_MS);
    Eye r = l;
    r.mirror = true;
    r.dx = l.dx - 8 * k;   // the right eye sits a bit closer
    face(c, l, r);
}

// keyboard_peek: a big keyboard in front, Clawd peeks over the edge

static const int KP_EDGE = 40;         // top edge of the keyboard
static const uint32_t KP_IN = 450;     // eyes sink, keyboard rises
static const int KP_KEYS = 17;         // 8 big keys with labels, 9 cut off below

static void kp_key(int k, int oy, int& x, int& y) {
    if (k < 8) {
        x = 1 + k * 16;
        y = KP_EDGE + 4 + oy;
    } else {
        x = -7 + (k - 8) * 16;
        y = KP_EDGE + 18 + oy;
    }
}

static void kp_keyboard(Canvas& c, int oy, int down) {
    static const char LABELS[] = "asdfjkl;";
    c.fill_rect(0, KP_EDGE + oy, 127, 63, false);
    c.fill_rect(0, KP_EDGE + oy, 127, KP_EDGE + 1 + oy);
    for (int k = 0; k < KP_KEYS; k++) {
        int x, y;
        kp_key(k, oy, x, y);
        int p = k == down ? 1 : 0;
        if (p) c.fill_rect(x, y + 1, x + 13, y + 12);
        else c.rect(x, y, x + 13, y + 11);
        if (k < 8) {
            char s[2] = {LABELS[k], 0};
            c.text(x + 5, y + 3 + p, s, font_4x6, !p);
        }
    }
}

static float kp_key_x(int k) {
    int x, y;
    kp_key(k, 0, x, y);
    return x + 7;
}

// a claw holding on to the edge, two tips hanging over the front
static void kp_claw(Canvas& c, int cx, int oy) {
    int y = KP_EDGE + oy;
    c.fill_rect(cx - 3, y - 4, cx + 3, y - 1);
    c.px(cx, y - 4, false);
    c.vline(cx - 2, y + 2, y + 3);
    c.vline(cx + 2, y + 2, y + 3);
}

static void a_keyboard_peek(Canvas& c, uint32_t t, const Ctx& x) {
    int rate = key_rate(x);
    float k = ease(seg(t, 0, KP_IN));
    uint32_t every = 1000 / rate;
    uint32_t in = t > KP_IN ? t - KP_IN : 0;
    uint32_t n = in / every;
    // mostly the big labelled keys, sometimes one on the row below
    uint32_t h = hash32(n * 2654435761u + x.seed);
    int key = h % 4 ? (h >> 4) % 8 : 8 + (h >> 4) % 9;
    int down = (t >= KP_IN && in % every < 120) ? key : -1;

    // peek-a-boo rounds: pop up somewhere, watch, duck, come up somewhere else
    uint32_t round_ms = 2700 + x.seed % 600;
    uint32_t r = in / round_ms, rt = in % round_ms * 2700 / round_ms;
    float spot = r ? (hashf(r * 7919 + x.seed) - 0.5f) * 44 : 0;   // -22..22 px
    const Key rise[] = {{0, 30}, {160, 0}, {260, 5}, {2300, 5}, {2420, 30}, {2700, 30}};
    float dy = KEYS(rise, rt);
    if (t < KP_IN || (r == 0 && rt < 260)) dy = 5 * k;   // the first round starts already peeking

    // a key goes down near him: a little startled hop, eyes glance at it
    float dx = 0;
    if (down >= 0) {
        float kx = kp_key_x(down);
        dx = clampf((kx - 64 - spot) / 10, -3, 3);
        if (kx > 30 + spot && kx < 98 + spot) dy -= 3;
    }
    // now and then a curious stretch up to see better
    uint32_t look = (r * 1237 + x.seed) % 1300 + 500;
    if (rt > look && rt < look + 600)
        dy -= 5 * ease(seg(rt, look, look + 150)) * (1 - ease(seg(rt, look + 450, look + 600)));

    Eye e = eye(spot + dx, dy, EYE_W + 1, EYE_H);
    if (rate >= 11) {   // so fast: eyes go wide
        e.w += 2;
        e.h += 2;
    }
    uint32_t period = 2300 + x.seed % 700;
    uint32_t bt = in % period;
    if (t >= KP_IN && bt > period - BLINK_MS) e = blink_eye(e, (bt - (period - BLINK_MS)) / (float)BLINK_MS);
    face(c, e);
    int oy = iround(26 * (1 - k));
    kp_keyboard(c, oy, down);
    if (dy < 16 && k > 0.8f) {   // he holds on while he is up
        kp_claw(c, iround(LEFT_X - 18 + spot), oy);
        kp_claw(c, iround(RIGHT_X + 18 + spot), oy);
    }
}

static const Anim A_YOU_TYPING = {"you_typing", a_you_typing, 4000, LOOP | CALM_IN};
static const Anim A_TYPING_READY = {"typing_ready", a_typing_ready, 1200, CALM_OUT};
static const Anim A_TYPING_WAKE = {"typing_wake", a_typing_wake, 700, CALM_OUT};
static const Anim A_TYPING_ALONG = {"typing_along", a_typing_along, 4000, LOOP | SCENE};
static const Anim A_LETTERS_FLY = {"letters_fly", a_letters_fly, 4800, LOOP | CALM_IN};
static const Anim A_KEYBOARD_PEEK = {"keyboard_peek", a_keyboard_peek, 5400, LOOP | CALM_IN};

ANIM_LIST(anims_keys, &A_YOU_TYPING, &A_TYPING_READY, &A_TYPING_WAKE, &A_TYPING_ALONG, &A_LETTERS_FLY,
          &A_KEYBOARD_PEEK);
