// whack a bug: Clawd hops along the three holes with a hammer by himself.
// tap to bonk when he is over a bug. don't bonk his little friend.
#include <stdio.h>

#include "clawd.h"
#include "game.h"
#include "motion.h"

static const int HOLES = 3;
static const int HX[HOLES] = {22, 64, 106};
static const int GROUND = 55;      // top of the dirt
static const int TOPY = 11;        // mini Clawd rides up here
static const int LIVES = 3;
static const uint32_t MOVE_MS = 250, RISE_MS = 220, SWING_MS = 350, DIZZY_MS = 700;

enum Who : uint8_t { NONE, BUG, FRIEND };

struct Hole {
    Who who;
    uint32_t at, stay;   // popped up at, time it stays up
    uint32_t bonk_at;    // when it got hit, 0 if not
};

static Hole holes[HOLES];
static uint32_t rng, last, last_input, died, t_leg, swing_at, dizzy_at, ouch_at, miss_at, next_pop, bot_until, bot_at;
static int pos, next_pos, step_dir, points, lives, best;
static bool dead, new_best;
static float hx;

static const uint8_t HEART[] = {0x6c, 0xfe, 0xfe, 0x7c, 0x38, 0x10};
static const uint8_t HEART_EMPTY[] = {0x6c, 0x92, 0x82, 0x44, 0x28, 0x10};

static uint32_t rnd() {
    rng = rng * 1664525u + 1013904223u;
    return rng >> 8;
}

// how long Clawd waits over each hole, a bit less as you score
static uint32_t dwell() { return points > 30 ? 550 : 850 - points * 10; }

static void whack_start(uint32_t now, uint32_t seed, int b) {
    rng = seed ? seed : 1;
    last = last_input = t_leg = now;
    pos = 1;
    next_pos = 2;
    step_dir = 1;
    hx = HX[1];
    points = 0;
    lives = LIVES;
    best = b;
    swing_at = dizzy_at = ouch_at = miss_at = bot_until = bot_at = 0;
    next_pop = now + 700;
    dead = new_best = false;
    for (Hole& h : holes) h = Hole{NONE, 0, 0, 0};
}

static void lose(uint32_t now) {
    if (--lives > 0) return;
    dead = true;
    died = now;
    new_best = points > best;
    if (new_best) best = points;
}

// 0..1 how far out of the hole it is
static float height(const Hole& h, uint32_t now) {
    if (h.who == NONE) return 0;
    uint32_t u = now - h.at;
    if (u < RISE_MS) return ease_out((float)u / RISE_MS);
    if (u < RISE_MS + h.stay) return 1;
    return 1 - ease_in((float)(u - RISE_MS - h.stay) / RISE_MS);
}

static void whack_input(Btn b, uint32_t now) {
    last_input = now;
    if (dead) {
        if (b == BTN_DOWN && now - died > 600) whack_start(now, rng, best);
        return;
    }
    if (b != BTN_DOWN) return;
    if (swing_at && now - swing_at < SWING_MS) return;
    if (dizzy_at && now - dizzy_at < DIZZY_MS) return;
    swing_at = now;
    // the hole under the hammer, generous while hopping
    Hole* h = nullptr;
    for (int i = 0; i < HOLES; i++)
        if (hx > HX[i] - 16 && hx < HX[i] + 16) h = &holes[i];
    if (h && h->who != NONE && !h->bonk_at && height(*h, now) > 0.2f) {
        h->bonk_at = now;
        if (h->who == BUG) {
            points++;
        } else {
            ouch_at = now;
            lose(now);
        }
    } else {
        dizzy_at = now;   // bonked the dirt, sees stars for a moment
    }
}

static void pop(uint32_t now) {
    int up = 0;
    for (const Hole& h : holes) up += h.who != NONE;
    if (up >= (points < 10 ? 1 : 2)) {
        next_pop = now + 300;
        return;
    }
    // pop where he is heading, so there is always time to get there
    bool sitting = now - t_leg < dwell() / 2;
    int i = sitting && rnd() % 3 == 0 ? pos : next_pos;
    if (holes[i].who != NONE) return;
    Hole& h = holes[i];
    h.who = points >= 5 && rnd() % 100 < 18 ? FRIEND : BUG;
    h.at = now;
    h.stay = (points > 30 ? 1700 : 2600 - points * 30) - (h.who == FRIEND ? 800 : 0);
    h.bonk_at = 0;
    next_pop = now + 900 + rnd() % 700 - (points > 20 ? 300 : points * 15);
}

static void update(uint32_t now) {
    // hop hole to hole, left right left, waiting a moment over each.
    // dizzy stops the clock so he stays put.
    if (dizzy_at && now - dizzy_at < DIZZY_MS) t_leg += now - last;
    last = now;
    uint32_t u = now - t_leg;
    if (u >= dwell() + MOVE_MS) {
        pos = next_pos;
        if (pos + step_dir < 0 || pos + step_dir >= HOLES) step_dir = -step_dir;
        next_pos = pos + step_dir;
        t_leg = now;
        u = 0;
    }
    float k = u < dwell() ? 0 : ease((float)(u - dwell()) / MOVE_MS);
    hx = HX[pos] + (HX[next_pos] - HX[pos]) * k;

    for (int i = 0; i < HOLES; i++) {
        Hole& h = holes[i];
        if (h.who == NONE) continue;
        uint32_t gone = h.bonk_at ? h.bonk_at + 400 : h.at + RISE_MS * 2 + h.stay;
        if ((int32_t)(now - gone) < 0) continue;
        if (h.who == BUG && !h.bonk_at) {
            miss_at = now;
            lose(now);
        }
        h.who = NONE;
    }
    if ((int32_t)(now - next_pop) >= 0) pop(now);
}

// a round little beetle, y is the tip of its feelers
static void bug(Canvas& c, int x, int y) {
    c.line(x - 2, y + 3, x - 4, y + 1);
    c.line(x + 2, y + 3, x + 4, y + 1);
    c.fill_rect(x - 5, y, x - 4, y + 1);
    c.fill_rect(x + 4, y, x + 5, y + 1);
    c.hline(x - 3, x + 3, y + 3);
    c.hline(x - 5, x + 5, y + 4);
    c.fill_rect(x - 6, y + 5, x + 6, y + 13);
    c.fill_rect(x - 3, y + 6, x - 2, y + 7, false);   // eyes
    c.fill_rect(x + 2, y + 6, x + 3, y + 7, false);
    c.vline(x, y + 9, y + 13, false);                 // shell split
    c.px(x - 3, y + 11, false);
    c.px(x + 3, y + 11, false);
}

static void hole_front(Canvas& c, int x) {
    c.fill_rect(x - 12, GROUND, x + 12, GROUND + 3);
    c.fill_rect(x - 9, GROUND + 1, x + 9, GROUND + 2, false);
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

static bool whack_draw(Canvas& c, uint32_t now) {
    if (!dead) update(now);

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

    // who is up, drawn first so the dirt hides the part still in the hole
    for (int i = 0; i < HOLES; i++) {
        const Hole& h = holes[i];
        if (h.who == NONE) continue;
        int x = HX[i];
        if (h.bonk_at) {
            if (h.who == BUG) {   // flat as a pancake
                c.fill_rect(x - 8, GROUND - 3, x + 8, GROUND - 1);
                for (int s = -1; s <= 1; s++) c.line(x + s * 6, GROUND - 6, x + s * 9, GROUND - 10);
            } else {
                clawd(c, x - 9, GROUND - 12, pose(EYES_CLOSED, ARMS_UP), 0.5f);
                c.text(x + 10, GROUND - 16, "ow", font_4x6);
            }
            continue;
        }
        float up = height(h, now);
        if (h.who == BUG) bug(c, x, GROUND - iround(15 * up));
        else clawd(c, x - 9, GROUND - iround(13 * up), pose(EYES_OPEN, up >= 1 ? ARMS_UP : ARMS_DOWN), 0.5f);
    }
    c.hline(0, 127, GROUND + 4);
    for (int i = 0; i < HOLES; i++) hole_front(c, HX[i]);

    // Clawd up top with the hammer hanging under him
    int x = iround(hx);
    uint32_t su = now - swing_at;
    bool swinging = swing_at && su < SWING_MS;
    float drop = swinging ? (su < 90 ? su / 90.0f : 1 - seg(su, 200, SWING_MS)) : 0;
    int head = TOPY + 16 + iround(drop * (GROUND - 12 - TOPY - 16));
    c.vline(x, TOPY + 12, head);
    c.fill_rect(x - 6, head, x + 6, head + 5);
    c.hline(x - 5, x + 5, head + 2, false);
    for (int s = -1; s <= 1; s += 2) {   // round the corners off
        c.px(x + s * 6, head, false);
        c.px(x + s * 6, head + 5, false);
    }
    bool dizzy = dizzy_at && now - dizzy_at < DIZZY_MS;
    bool hurt = ouch_at && now - ouch_at < 600;
    Eyes e = dizzy || hurt ? EYES_CLOSED : EYES_OPEN;
    clawd(c, x - 9, TOPY, pose(e, swinging ? ARMS_UP : ARMS_DOWN, FEET_BOTH), 0.5f);
    if (dizzy) {
        float a = (now - dizzy_at) * 0.012f;
        for (int s = 0; s < 2; s++) {
            int sx = x + iround(10 * cosf(a + s * 3.14f)), sy = TOPY - 2 + iround(2 * sinf(a + s * 3.14f));
            c.hline(sx - 1, sx + 1, sy);
            c.vline(sx, sy - 1, sy + 1);
        }
    }
    if (miss_at && now - miss_at < 700) c.text_center(64, 1, "got away!", font_4x6);

    c.fill_rect(0, 0, LIVES * 9, 7, false);
    for (int i = 0; i < LIVES; i++) c.blit(1 + i * 9, 1, 7, 6, i < lives ? HEART : HEART_EMPTY);
    snprintf(buf, sizeof(buf), "%d", points);
    c.text(127 - c.text_width(buf), 0, buf);
    return true;
}

static int whack_score() { return points; }

// bot: waits till the hammer is over a bug, then bonks a little later
static bool whack_bot(uint32_t now) {
    if (dead) return false;
    if (now < bot_until) return true;
    if (now - bot_until < 400) return false;
    for (int i = 0; i < HOLES; i++) {
        const Hole& h = holes[i];
        bool over = hx > HX[i] - 3 && hx < HX[i] + 3;
        if (!over || h.who != BUG || h.bonk_at || height(h, now) < 1) continue;
        if (!bot_at) bot_at = now + 80 + hash32(now) % (points > 25 ? 200 + (points - 25) * 25 : 200);   // tired later on
        if (now >= bot_at) {
            bot_at = 0;
            bot_until = now + 60;
            return true;
        }
        return false;
    }
    bot_at = 0;
    return false;
}

extern const Game game_whack = {"whack", "whack a bug", whack_start, whack_input, whack_draw, whack_score, whack_bot};
