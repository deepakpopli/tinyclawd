// token catch: Clawd walks by himself, tap to turn him around.
// catch the falling tokens, dodge the bugs. hearts give a life back.
#include <stdio.h>

#include "clawd.h"
#include "game.h"
#include "motion.h"

static const int GROUND = 63;
static const int CY = GROUND - SPRITE_H + 1;   // top of Clawd
static const int LIVES = 3;
static const int MAX_ITEMS = 4;
static const uint32_t OUCH_MS = 1000;

enum Kind : uint8_t { TOKEN, HEART, BUG };

struct Item {
    float x, y;   // centre
    Kind kind;
    bool alive;
};

static Item items[MAX_ITEMS];
static uint32_t rng, last, died, last_input, next_drop, happy_at, ouch_at, miss_at, bot_until;
static float cx;
static int dir, points, lives, best;
static bool dead, new_best;

static const uint8_t TOKEN_BITS[] = {0x38, 0x44, 0x92, 0xba, 0x92, 0x44, 0x38};
static const uint8_t HEART_BITS[] = {0x6c, 0xfe, 0xfe, 0x7c, 0x38, 0x10};
static const uint8_t HEART_EMPTY[] = {0x6c, 0x92, 0x82, 0x44, 0x28, 0x10};
static const uint8_t BUG_BITS[] = {0x82, 0x44, 0x7c, 0xfe, 0x7c, 0xaa};

static uint32_t rnd() {
    rng = rng * 1664525u + 1013904223u;
    return rng >> 8;
}

// px / ms, both speed up a little as the score goes up
static float walk() { return clampf(0.035f + points * 0.0002f, 0, 0.05f); }
static float fall() { return clampf(0.016f + points * 0.0002f, 0, 0.028f); }

static void catch_start(uint32_t now, uint32_t seed, int b) {
    rng = seed ? seed : 1;
    last = last_input = now;
    cx = 64 - SPRITE_W / 2;
    dir = 1;
    points = 0;
    lives = LIVES;
    best = b;
    next_drop = now + 600;
    happy_at = ouch_at = miss_at = bot_until = 0;
    dead = new_best = false;
    for (Item& it : items) it.alive = false;
}

static void catch_input(Btn b, uint32_t now) {
    last_input = now;
    if (dead) {
        if (b == BTN_DOWN && now - died > 600) catch_start(now, rng, best);
        return;
    }
    if (b == BTN_DOWN) dir = -dir;
}

static void drop(uint32_t now) {
    Item* it = nullptr;
    for (Item& s : items)
        if (!s.alive) it = &s;
    if (!it) return;
    uint32_t r = rnd() % 100;
    it->kind = points >= 8 && r < 22 ? BUG : (r > 92 && lives < LIVES ? HEART : TOKEN);
    if (it->kind == BUG) {
        it->x = 6 + rnd() % 116;
    } else {
        // always somewhere he can still get to in time
        float reach = (CY + 4) / fall() * walk() * 0.7f;
        float mid = cx + SPRITE_W / 2.0f;
        it->x = clampf(mid - reach + (rnd() % 1000) / 1000.0f * 2 * reach, 6, 121);
    }
    it->y = -4;
    it->alive = true;
    int gap = 1800 - points * 25;
    next_drop = now + (gap < 1000 ? 1000 : gap) + rnd() % 400;
}

static void lose(uint32_t now) {
    if (--lives > 0) return;
    dead = true;
    died = now;
    new_best = points > best;
    if (new_best) best = points;
}

static void step(float dt, uint32_t now) {
    cx += dir * walk() * dt;
    if (cx < 0) cx = 0, dir = 1;
    if (cx > 127 - SPRITE_W) cx = 127 - SPRITE_W, dir = -1;
    for (Item& it : items) {
        if (!it.alive) continue;
        it.y += fall() * dt;
        bool in = it.x > cx + 1 && it.x < cx + SPRITE_W - 1;
        if (in && it.y + 3 >= CY && it.y < CY + 8) {
            it.alive = false;
            if (it.kind == BUG) {
                ouch_at = now;
                lose(now);
            } else {
                happy_at = now;
                if (it.kind == HEART) lives++;
                else points++;
            }
        } else if (it.y > GROUND - 3) {
            it.alive = false;
            if (it.kind == TOKEN) {
                miss_at = now;
                lose(now);
            }
        }
        if (dead) return;
    }
    if ((int32_t)(now - next_drop) >= 0) drop(now);
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

static bool catch_draw(Canvas& c, uint32_t now) {
    if (!dead) {
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

    for (const Item& it : items) {
        if (!it.alive) continue;
        int x = iround(it.x), y = iround(it.y);
        if (it.kind == TOKEN) c.blit(x - 3, y - 3, 7, 7, TOKEN_BITS);
        else if (it.kind == HEART) c.blit(x - 3, y - 3, 7, 6, HEART_BITS);
        else c.blit(x - 3, y - 3, 7, 6, BUG_BITS);
    }

    bool happy = happy_at && now - happy_at < 300;
    bool ouch = ouch_at && now - ouch_at < OUCH_MS;
    Eyes e = ouch ? EYES_CLOSED : (happy ? EYES_OPEN : (dir < 0 ? EYES_LEFT : EYES_RIGHT));
    Feet f = (now / 140) & 1 ? FEET_LEFT : FEET_RIGHT;
    if (!ouch || (now / 100) & 1) clawd(c, iround(cx), CY - (happy ? 2 : 0), pose(e, happy ? ARMS_UP : ARMS_DOWN, f));
    if (happy) c.text(iround(cx) + SPRITE_W / 2 - 5, CY - 10, "+1", font_4x6);
    if (miss_at && now - miss_at < 600) c.text_center(64, 12, "missed one", font_4x6);

    // hud on little black patches, things fall behind it
    c.fill_rect(0, 0, LIVES * 9, 7, false);
    for (int i = 0; i < LIVES; i++) c.blit(1 + i * 9, 1, 7, 6, i < lives ? HEART_BITS : HEART_EMPTY);
    snprintf(buf, sizeof(buf), "%d", points);
    int w = c.text_width(buf);
    c.fill_rect(125 - w, 0, 127, 8, false);
    c.text(127 - w, 0, buf);
    return true;
}

static int catch_score() { return points; }

// bot: walks to the token that lands first, turns away from bugs about to land on him
static bool catch_bot(uint32_t now) {
    if (dead) return false;
    if (now < bot_until) return true;
    if (now - bot_until < 400) return false;   // a person waits to see the turn
    float mid = cx + SPRITE_W / 2.0f;
    const Item* goal = nullptr;
    for (const Item& it : items)
        if (it.alive && it.kind != BUG && (!goal || it.y > goal->y)) goal = &it;
    int want = dir;
    if (goal && (goal->x - mid) * dir < -6) want = -dir;
    for (const Item& it : items) {
        if (!it.alive || it.kind != BUG || it.y < CY - 22) continue;
        float ahead = mid + dir * 14;
        if (it.x > ahead - 22 && it.x < ahead + 22) want = it.x > mid ? -1 : 1;
    }
    // sometimes he daydreams
    if (want != dir && hashf(now / 700 + points * 31) > 0.08f + points * 0.004f) {
        bot_until = now + 60;
        return true;
    }
    return false;
}

// menu logo: a token falling into his arms
static void catch_icon(Canvas& c, int cx, int cy, uint32_t now) {
    int fall = (now / 150) % 5;
    int ty = cy - 9 + fall;
    // a round token with a slot, like the ones in the game but bigger
    c.ellipse(cx - 4, ty - 4, cx + 4, ty + 4);
    c.ellipse(cx - 3, ty - 3, cx + 3, ty + 3);
    c.vline(cx, ty - 2, ty + 2);
    // two little sparkles
    c.hline(cx - 9, cx - 7, ty - 3);
    c.vline(cx - 8, ty - 4, ty - 2);
    c.hline(cx + 7, cx + 9, ty + 1);
    c.vline(cx + 8, ty, ty + 2);
    clawd(c, cx - 9, cy + 1, pose(EYES_OPEN, ARMS_UP, FEET_BOTH), 0.5f);
}

extern const Game game_catch = {"catch", "token catch", catch_start, catch_input, catch_draw, catch_score, catch_bot, catch_icon};
