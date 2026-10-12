// bug run: Clawd runs right, jump the bugs and walls, grab tokens.
#include <stdio.h>

#include "clawd.h"
#include "game.h"
#include "motion.h"

static const int GROUND = 60;
static const int CX = 12;               // Clawd's left edge
static const int CW = 18, CH = 12;      // mini Clawd at scale 0.5
static const float GRAV = 0.0009f;      // px / ms^2
static const float GRAV_HELD = 0.0004f;
static const float JUMP_V = -0.17f;     // px / ms
static const int MAX_OBS = 6;

enum Kind : uint8_t { BUG, WALL, STACK, TOKEN };

struct Obs {
    float x;
    Kind kind;
    bool alive;
};

static Obs obs[MAX_OBS];
static uint32_t bot_until, bot_tired;
static uint32_t rng, t0, last, died, last_input;
static float dist, y, vy, next_gap;
static int tokens, best, final_score;
static bool held, dead, new_best;

static uint32_t rnd() {
    rng = rng * 1664525u + 1013904223u;
    return rng >> 8;
}

static float speed() { return clampf(0.06f + dist * 0.0000045f, 0.06f, 0.14f); }

static int w_of(Kind k) { return k == BUG ? 8 : (k == TOKEN ? 5 : (k == WALL ? 15 : 6)); }
static int h_of(Kind k) { return k == BUG ? 5 : (k == TOKEN ? 5 : (k == WALL ? 9 : 14)); }

static int run_score() { return (int)(dist / 10) + tokens; }

static void run_start(uint32_t now, uint32_t seed, int b) {
    rng = seed ? seed : 1;
    t0 = last = last_input = now;
    dist = 0;
    y = 0;
    vy = 0;
    next_gap = 60;
    tokens = 0;
    best = b;
    held = dead = new_best = false;
    bot_tired = now + 20000 + rnd() % 20000;   // the bot gets sloppy after this
    for (Obs& o : obs) o.alive = false;
}

static void run_input(Btn b, uint32_t now) {
    last_input = now;
    if (dead) {
        if (b == BTN_DOWN && now - died > 600) run_start(now, rng, best);
        return;
    }
    if (b == BTN_DOWN) {
        held = true;
        if (y == 0) vy = JUMP_V;
    } else if (b == BTN_UP) {
        held = false;
        if (vy < -0.11f) vy = -0.11f;   // let go early, short hop
    }
}

static void spawn() {
    Obs* o = nullptr;
    for (Obs& s : obs)
        if (!s.alive) o = &s;
    if (!o) return;
    uint32_t r = rnd() % 10;
    o->kind = r < 5 ? BUG : (r < 7 ? WALL : (r < 8 ? STACK : TOKEN));
    if (dist < 400 && o->kind != BUG) o->kind = r < 7 ? BUG : TOKEN;   // easy start
    o->x = 130;
    o->alive = true;
    // gap grows with speed so every jump stays possible
    next_gap = 45 + speed() * 400 + rnd() % 50;
}

static void step(float dt) {
    float v = speed();
    dist += v * dt;
    for (Obs& o : obs)
        if (o.alive) {
            o.x -= v * dt;
            if (o.x < -16) o.alive = false;
        }
    next_gap -= v * dt;
    if (next_gap <= 0) spawn();

    if (y < 0 || vy < 0) {
        vy += (held && vy < 0 ? GRAV_HELD : GRAV) * dt;
        y += vy * dt;
        if (y >= 0) y = vy = 0;
    }

    // hits, a little forgiving
    int top = GROUND - CH + iround(y);
    for (Obs& o : obs) {
        if (!o.alive) continue;
        int ox = iround(o.x), w = w_of(o.kind), h = h_of(o.kind);
        int oy = o.kind == TOKEN ? GROUND - 22 : GROUND - h;
        bool hit = ox < CX + CW - 3 && ox + w > CX + 3 && oy < top + CH - 1 && oy + h > top + 2;
        if (!hit) continue;
        if (o.kind == TOKEN) {
            tokens += 5;
            o.alive = false;
        } else {
            dead = true;
            final_score = run_score();
            new_best = final_score > best;
            if (new_best) best = final_score;
            return;
        }
    }
}

static void draw_obs(Canvas& c, const Obs& o, uint32_t now) {
    int x = iround(o.x);
    switch (o.kind) {
    case BUG: {
        // round shell, head, legs that wiggle
        c.fill_ellipse(x + 1, GROUND - 5, x + 7, GROUND - 1);
        c.vline(x + 4, GROUND - 5, GROUND - 1, false);
        c.fill_rect(x, GROUND - 3, x, GROUND - 2);
        c.px(x - 1, GROUND - 5);
        bool k = (now / 120) & 1;
        for (int i = 0; i < 3; i++) c.px(x + 2 + i * 2 + (k ? 1 : 0), GROUND);
        break;
    }
    case WALL:
        c.rect(x, GROUND - 9, x + 14, GROUND);
        c.text(x + 2, GROUND - 7, "429", font_4x6);
        break;
    case STACK:
        for (int i = 0; i < 2; i++) c.text(x, GROUND - 13 + i * 7, "}", font_5x7);
        break;
    case TOKEN: {
        int yy = GROUND - 22 + (((now / 200) & 1) ? 1 : 0);
        c.rect(x, yy, x + 4, yy + 4);
        c.px(x + 2, yy + 2);
        break;
    }
    }
}

static void dead_face(Canvas& c, int x, int y0) {
    clawd(c, x, y0, pose(EYES_OPEN, ARMS_UP, FEET_BOTH));
    // wipe the eyes and draw x x
    for (int e = 0; e < 2; e++) {
        int ex = x + 8 + e * 16;
        c.fill_rect(ex - 2, y0 + 1, ex + 2, y0 + 5, true);
        c.line(ex - 2, y0 + 1, ex + 2, y0 + 5, 1, false);
        c.line(ex + 2, y0 + 1, ex - 2, y0 + 5, 1, false);
    }
}

static bool run_draw(Canvas& c, uint32_t now) {
    if (!dead) {
        uint32_t dt = now - last;
        if (dt > 100) dt = 100;
        // small fixed steps so a slow frame never skips through a bug
        while (dt > 0 && !dead) {
            uint32_t s = dt > 8 ? 8 : dt;
            step((float)s);
            dt -= s;
        }
        if (dead) died = now;
    }
    last = now;

    char buf[24];
    if (dead) {
        dead_face(c, 46, 4);
        snprintf(buf, sizeof(buf), "score %d", final_score);
        c.text_center(64, 32, buf);
        snprintf(buf, sizeof(buf), "best %d", best);
        c.text_center(64, 42, buf);
        if (new_best && (now - died) % 600 < 420) c.text_center(64, 53, "new best!");
        return now - (last_input > died ? last_input : died) < 6000;
    }

    // ground with little pebbles scrolling by
    c.hline(0, 127, GROUND + 1);
    int off = (int)dist % 23;
    for (int px = -off; px < 128; px += 23) c.px(px + 7, GROUND + 3);
    for (int px = -((int)dist % 37); px < 128; px += 37) c.hline(px + 20, px + 21, GROUND + 2);

    for (const Obs& o : obs)
        if (o.alive) draw_obs(c, o, now);

    Feet f = y < 0 ? FEET_BOTH : (((uint32_t)(dist / 6)) & 1 ? FEET_LEFT : FEET_RIGHT);
    clawd(c, CX, GROUND - CH + 1 + iround(y), pose(EYES_RIGHT, y < 0 ? ARMS_UP : ARMS_DOWN, f), 0.5f);

    snprintf(buf, sizeof(buf), "%d", run_score());
    c.text(127 - c.text_width(buf), 0, buf);
    snprintf(buf, sizeof(buf), "best %d", best);
    c.text(0, 0, buf, font_4x6);
    return true;
}

static int run_score_fn() { return dead ? final_score : run_score(); }

// bot: looks at the next thing in front and jumps for it
static bool run_bot(uint32_t now) {
    if (dead) return false;
    if (now < bot_until) return true;
    if (y < 0) return false;
    float v = speed();
    for (const Obs& o : obs) {
        if (!o.alive || o.kind == TOKEN) continue;
        float gap = o.x - (CX + CW);
        // a bit of noise in when he reacts, more as it speeds up, so he dies some day
        float lead = 1 + v * 50 + (hashf(now / 500 + (uint32_t)o.x) - 0.5f) * (now > bot_tired ? 30 : 4);
        if (gap > 0 && gap < lead) {
            bot_until = now + (o.kind == STACK ? 320 : o.kind == WALL ? 220 : 140);
            return true;
        }
    }
    return false;
}

extern const Game game_run = {"run", "bug run", run_start, run_input, run_draw, run_score_fn, run_bot};
