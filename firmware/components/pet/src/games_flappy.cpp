// flappy clawd: tap to flap through the gaps. wide gaps, slow scroll, 3 hearts.
#include <stdio.h>

#include "clawd.h"
#include "game.h"
#include "motion.h"

static const int GROUND = 60;
static const int CX = 22;             // Clawd's left edge
static const int CW = 18, CH = 12;    // mini Clawd at scale 0.5
static const int PW = 10;             // pipe width
static const int GAP = 30;            // gap height, big on purpose
static const float GRAV = 0.00012f;    // px / ms^2
static const float FLAP_V = -0.05f;   // px / ms
static const float MAX_FALL = 0.06f;
static const float SPEED = 0.035f;    // scroll, px / ms
static const int LIVES = 3;
static const uint32_t SAFE_MS = 1500;
static const int MAX_PIPES = 3;

struct Pipe {
    float x;
    int mid;      // centre of the gap
    bool passed;
};

static Pipe pipes[MAX_PIPES];
static uint32_t rng, last, died, last_input, flap_at, hit_at, bot_until;
static float y, vy, dist, next_pipe;
static int points, lives, best, last_mid;
static bool ready, dead, new_best;

static const uint8_t HEART[] = {0x6c, 0xfe, 0xfe, 0x7c, 0x38, 0x10};
static const uint8_t HEART_EMPTY[] = {0x6c, 0x92, 0x82, 0x44, 0x28, 0x10};

static uint32_t rnd() {
    rng = rng * 1664525u + 1013904223u;
    return rng >> 8;
}

static void flappy_start(uint32_t now, uint32_t seed, int b) {
    rng = seed ? seed : 1;
    last = last_input = now;
    y = 30;
    vy = 0;
    dist = 0;
    next_pipe = 40;
    points = 0;
    lives = LIVES;
    best = b;
    last_mid = 34;
    flap_at = hit_at = bot_until = 0;
    ready = true;
    dead = new_best = false;
    for (Pipe& p : pipes) p.x = -100;
}

static void flappy_input(Btn b, uint32_t now) {
    last_input = now;
    if (dead) {
        if (b == BTN_DOWN && now - died > 600) flappy_start(now, rng, best);
        return;
    }
    if (b != BTN_DOWN) return;
    ready = false;
    vy = FLAP_V;
    flap_at = now;
}

static void spawn() {
    for (Pipe& p : pipes) {
        if (p.x > -PW) continue;
        // the gap moves a bit from the last one, pulled to the middle
        int lo = GAP / 2 + 4, hi = GROUND - GAP / 2 - 4;
        int mid = (last_mid + (lo + hi) / 2) / 2 + (int)(rnd() % 21) - 10;
        mid = mid < lo ? lo : (mid > hi ? hi : mid);
        last_mid = mid;
        p.x = 128;
        p.mid = mid;
        p.passed = false;
        break;
    }
    next_pipe = 72 + rnd() % 16;
}

static void hurt(uint32_t now) {
    if (hit_at && now - hit_at < SAFE_MS) return;
    hit_at = now;
    if (--lives > 0) return;
    dead = true;
    died = now;
    new_best = points > best;
    if (new_best) best = points;
}

static void step(float dt, uint32_t now) {
    vy += GRAV * dt;
    if (vy > MAX_FALL) vy = MAX_FALL;
    y += vy * dt;
    if (y < 0) y = 0, vy = 0;
    if (y > GROUND - CH) {
        // the ground is soft, a little bounce and no harm
        y = GROUND - CH;
        vy = FLAP_V;
    }
    dist += SPEED * dt;
    next_pipe -= SPEED * dt;
    if (next_pipe <= 0) spawn();
    for (Pipe& p : pipes) {
        if (p.x <= -PW) continue;
        p.x -= SPEED * dt;
        if (!p.passed && p.x + PW < CX + 4) {
            p.passed = true;
            points++;
        }
        // forgiving box, 3 px smaller than Clawd all round
        int px = iround(p.x);
        bool over = px < CX + CW - 3 && px + PW > CX + 3;
        bool out = y + 3 < p.mid - GAP / 2 || y + CH - 3 > p.mid + GAP / 2;
        if (over && out) hurt(now);
    }
}

static void pipe(Canvas& c, const Pipe& p) {
    int x = iround(p.x), a = p.mid - GAP / 2, b = p.mid + GAP / 2;
    // body with a lip at the gap end, like a little green pipe
    c.rect(x + 1, -1, x + PW - 2, a - 4);
    c.rect(x - 1, a - 4, x + PW, a);
    c.rect(x + 1, b + 4, x + PW - 2, GROUND);
    c.rect(x - 1, b, x + PW, b + 4);
    for (int yy = 1; yy < a - 5; yy += 3) c.px(x + 3, yy);
    for (int yy = b + 6; yy < GROUND; yy += 3) c.px(x + 3, yy);
}

static void dead_face(Canvas& c, int x, int y0) {
    clawd(c, x, y0, pose(EYES_OPEN, ARMS_UP, FEET_BOTH));
    for (int e = 0; e < 2; e++) {
        int ex = x + 8 + e * 16;
        c.fill_rect(ex - 2, y0 + 1, ex + 2, y0 + 5, true);
        c.line(ex - 2, y0 + 1, ex + 2, y0 + 5, 1, false);
        c.line(ex + 2, y0 + 1, ex - 2, y0 + 5, 1, false);
    }
}

static bool flappy_draw(Canvas& c, uint32_t now) {
    if (!dead && !ready) {
        uint32_t dt = now - last;
        if (dt > 100) dt = 100;
        while (dt > 0 && !dead) {
            uint32_t s = dt > 8 ? 8 : dt;
            step((float)s, now);
            dt -= s;
        }
    }
    last = now;

    char buf[24];
    if (dead) {
        dead_face(c, 46, 4);
        snprintf(buf, sizeof(buf), "score %d", points);
        c.text_center(64, 32, buf);
        snprintf(buf, sizeof(buf), "best %d", best);
        c.text_center(64, 42, buf);
        if (new_best && (now - died) % 600 < 420) c.text_center(64, 53, "new best!");
        return now - (last_input > died ? last_input : died) < 6000;
    }

    for (const Pipe& p : pipes)
        if (p.x > -PW) pipe(c, p);

    // ground with grass tufts going by
    c.hline(0, 127, GROUND + 1);
    for (int gx = -((int)dist % 16); gx < 128; gx += 16) {
        c.px(gx + 4, GROUND);
        c.px(gx + 6, GROUND);
        c.px(gx + 5, GROUND + 3);
    }

    // arms up right after a flap is the wing beat
    int yy = ready ? 30 + ((now / 400) & 1) : iround(y);
    bool flap = now - flap_at < 160 || (ready && (now / 200) & 1);
    bool blink = hit_at && now - hit_at < SAFE_MS;
    if (!blink || (now / 100) & 1) {
        Eyes e = blink ? EYES_CLOSED : (vy > 0.07f && !ready ? EYES_OPEN : EYES_RIGHT);
        c.fill_rect(CX - 1, yy - 1, CX + CW, yy + CH - 1, false);
        clawd(c, CX, yy, pose(e, flap ? ARMS_UP : ARMS_DOWN, FEET_BOTH), 0.5f);
    }
    if (ready) c.text_center(76, 46, "tap to flap", font_4x6);

    // hud on little black patches so the pipes can go behind it
    c.fill_rect(0, 0, LIVES * 9, 7, false);
    for (int i = 0; i < LIVES; i++) c.blit(1 + i * 9, 1, 7, 6, i < lives ? HEART : HEART_EMPTY);
    snprintf(buf, sizeof(buf), "%d", points);
    int w = c.text_width(buf);
    c.fill_rect(125 - w, 0, 127, 8, false);
    c.text(127 - w, 0, buf);
    return true;
}

static int flappy_score() { return points; }

// bot: flaps when he sinks under the middle of the next gap, then waits to
// see what the flap did, like a person would. gets sloppy later.
static bool flappy_bot(uint32_t now) {
    if (dead) return false;
    if (now < bot_until) return true;
    if (now - bot_until < 300) return false;
    if (ready && now - last_input < 900) return false;
    float target = 36, nearest = 999;
    for (const Pipe& p : pipes)
        if (p.x + PW > CX && p.x < nearest) {
            nearest = p.x;
            target = p.mid;
        }
    float wobble = points > 15 ? (hashf(now / 300) - 0.5f) * points * 0.8f : 0;
    if (ready || (y + CH / 2 > target + 3 + wobble && vy > -0.02f)) {
        bot_until = now + 60;
        return true;
    }
    return false;
}

extern const Game game_flappy = {"flappy", "flappy clawd", flappy_start, flappy_input, flappy_draw, flappy_score, flappy_bot};
