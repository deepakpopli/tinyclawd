// keepy uppy: head the ball up, don't let it drop.
#include <math.h>
#include <stdio.h>

#include "clawd.h"
#include "face.h"
#include "game.h"
#include "motion.h"

static const int GROUND = 63;
static const int HEAD = GROUND - SPRITE_H;   // top of Clawd
static const int WIN_TOP = HEAD - 24;        // ball y range that can be kicked, big on purpose
static const int WIN_BOT = HEAD + 2;
static const float R = 3;

static uint32_t rng, last, died, last_input, kicked_at, nice_at, whiff_at, saved_at, bot_until, bot_next;
static float bx, by, vx, vy, cx;
static int streak, kicks, best, final_score;
static bool dead, falling_out, new_best, bounced;

static uint32_t rnd() {
    rng = rng * 1664525u + 1013904223u;
    return rng >> 8;
}

static float gravity() { return fminf(0.00012f + kicks * 0.0000015f, 0.0002f); }

static void keepy_start(uint32_t now, uint32_t seed, int b) {
    rng = seed ? seed : 1;
    last = last_input = now;
    bx = 64;
    by = -4;
    vx = vy = 0;
    cx = 64 - SPRITE_W / 2;
    streak = kicks = 0;
    best = b;
    kicked_at = nice_at = whiff_at = saved_at = 0;
    bot_next = 0;
    dead = falling_out = new_best = bounced = false;
}

static void kick(uint32_t now) {
    float mid = (WIN_TOP + WIN_BOT) / 2.0f;
    bool perfect = fabsf(by - mid) <= 4;
    bounced = false;
    streak += perfect ? 2 : 1;
    kicks++;
    if (perfect) nice_at = now;
    kicked_at = now;
    // up to near the top of the screen, sideways at random
    float h = by - 13;
    vy = -sqrtf(2 * gravity() * h);
    vx = ((rnd() % 1000) / 1000.0f - 0.5f) * 0.07f;
}

static void keepy_input(Btn b, uint32_t now) {
    last_input = now;
    if (dead) {
        if (b == BTN_DOWN && now - died > 600) keepy_start(now, rng, best);
        return;
    }
    if (b != BTN_DOWN || falling_out) return;
    if (now - whiff_at < 250) return;   // no spamming
    if (vy > 0 && by >= WIN_TOP && by <= WIN_BOT && fabsf(bx - (cx + SPRITE_W / 2)) < 22) kick(now);
    else whiff_at = now;
}

static void step(float dt, uint32_t now) {
    vy += gravity() * dt;
    bx += vx * dt;
    by += vy * dt;
    if (bx < R) bx = R, vx = -vx;
    if (bx > 127 - R) bx = 127 - R, vx = -vx;
    if (by > WIN_BOT && vy > 0) falling_out = true;
    if (by > GROUND - R && !bounced) {
        // one free bounce: back up into the window for another go
        by = GROUND - R;
        bounced = true;
        falling_out = false;
        saved_at = now;
        vy = -sqrtf(2 * gravity() * (GROUND - R - (WIN_TOP - 6)));
        vx *= 0.5f;
    } else if (by > GROUND - R) {
        by = GROUND - R;
        dead = true;
        died = now;
        final_score = streak;
        new_best = streak > best;
        if (new_best) best = streak;
    }
    // Clawd shuffles under the ball
    float want = clampf(bx - SPRITE_W / 2.0f, 0, 127 - SPRITE_W);
    float mv = 0.09f * dt;
    if (!falling_out) cx += clampf(want - cx, -mv, mv);
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

static bool keepy_draw(Canvas& c, uint32_t now) {
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
        snprintf(buf, sizeof(buf), "score %d", final_score);
        c.text_center(64, 32, buf);
        snprintf(buf, sizeof(buf), "best %d", best);
        c.text_center(64, 42, buf);
        if (new_best && (now - died) % 600 < 420) c.text_center(64, 53, "new best!");
        return now - (last_input > died ? last_input : died) < 6000;
    }

    bool up = now - kicked_at < 180;
    bool walking = !falling_out && fabsf(bx - SPRITE_W / 2.0f - cx) > 2;
    Feet f = walking ? ((now / 120) & 1 ? FEET_LEFT : FEET_RIGHT) : FEET_BOTH;
    Eyes e = falling_out ? EYES_CLOSED : (bx < cx + 10 ? EYES_LEFT : (bx > cx + 26 ? EYES_RIGHT : EYES_OPEN));
    clawd(c, iround(cx), HEAD - (up ? 2 : 0), pose(e, up ? ARMS_UP : ARMS_DOWN, f));
    if (now - nice_at < 600 && nice_at) c.text_center(64, 14, "nice!");
    else if (bounced && now - saved_at < 900) c.text_center(64, 14, "boing! again!");
    snprintf(buf, sizeof(buf), "%d", streak);
    c.text(127 - c.text_width(buf, font_bold12), 0, buf, font_bold12);
    snprintf(buf, sizeof(buf), "best %d", best);
    c.text(0, 0, buf, font_4x6);

    c.fill_circle(bx, by, R + 1, false);   // keep it readable over the text
    ball(c, bx, by, R);
    return true;
}

static int keepy_score() { return dead ? final_score : streak; }

// bot: taps when the ball drops into the window, sometimes a bit late
static bool keepy_bot(uint32_t now) {
    if (dead) return false;
    if (now < bot_until) return true;
    if (vy <= 0 || by < WIN_TOP) {
        bot_next = 0;
        return false;
    }
    if (!bot_next) {
        // where in the window he will tap this time
        uint32_t h = hash32(kicked_at * 7 + streak + rng + (bounced ? 99 : 0));
        float late = kicks >= 30 && (h % 100) < (uint32_t)(kicks / 4) ? 30 : 0;
        bot_next = 1 + (uint32_t)(WIN_TOP + 6 + (h >> 8) % 9 + late);
    }
    if (by >= bot_next - 1) {
        bot_until = now + 80;
        bot_next = 0xffff;   // one tap per ball
        return true;
    }
    return false;
}

// menu logo: the ball bouncing on his head
static void keepy_icon(Canvas& c, int cx, int cy, uint32_t now) {
    int up = (now / 250) % 4;
    int by = cy - 6 - (up == 1 || up == 3 ? 2 : (up == 2 ? 3 : 0));
    // a football: round, a patch in the middle and bits of the others at the edge
    c.ellipse(cx - 4, by - 4, cx + 4, by + 4);
    c.fill_rect(cx - 1, by - 1, cx + 1, by + 1);
    c.hline(cx - 1, cx + 1, by - 3);
    c.px(cx - 3, by + 1);
    c.px(cx + 3, by + 1);
    c.hline(cx - 1, cx + 1, by + 3);
    clawd(c, cx - 9, cy + 1, pose(EYES_OPEN, ARMS_UP, FEET_BOTH), 0.5f);
    // a bump mark when the ball is down on his head
    if (up == 0) {
        c.px(cx - 6, by + 4);
        c.px(cx + 6, by + 4);
    }
}

extern const Game game_keepy = {"keepy", "keepy uppy", keepy_start, keepy_input, keepy_draw, keepy_score, keepy_bot, keepy_icon};
