// penalties: 5 rounds, you shoot then you keep.
// shooting: tap to lock the aim, tap to lock the power.
// keeping: Clawd sways on his own, tap to dive the way he is moving.
#include <math.h>
#include <stdio.h>

#include "clawd.h"
#include "game.h"

namespace {

const int ROUNDS = 5;
const int GL = 24, GR = 104, BAR = 12, GROUND = 56;

enum Phase { AIM, POWER, FLY, SHOT_DONE, KEEP, KEEP_DONE, OVER };

Phase phase;
uint32_t phase_t, rng, last_input;
int round_no, you, cpu, saves, best;
float aim, power, sway;
int kdir, sdir, dive_dir;
uint32_t shot_t, dive_t;
bool scored, over_bar, wide;
const char* say;
uint32_t bot_until;
float bot_aim, bot_power;

uint32_t rnd() {
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}
float frand() { return (rnd() % 1000) / 1000.0f; }

void go(Phase p, uint32_t now) {
    phase = p;
    phase_t = now;
}

float ease_out(float k) { return 1 - (1 - k) * (1 - k); }
float clampf(float v, float a, float b) { return v < a ? a : v > b ? b : v; }

int zone(float x) { return x < -0.33f ? -1 : x > 0.33f ? 1 : 0; }

float aim_at(uint32_t now) { return 1.1f * sinf((now - phase_t) * 0.0021f); }
float power_at(uint32_t now) {
    float k = fmodf((now - phase_t) / 1800.0f, 2.0f);
    return k < 1 ? k : 2 - k;
}
const float SWAY_MS = 2400;
float sway_at(uint32_t now) { return 0.8f * sinf((now - phase_t) * 6.2832f / SWAY_MS); }
int sway_dir(uint32_t now) { return cosf((now - phase_t) * 6.2832f / SWAY_MS) >= 0 ? 1 : -1; }

void start_keep(uint32_t now) {
    go(KEEP, now);
    sdir = (int)(rnd() % 3) - 1;
    // a long run up, the kicker looks where he will shoot (see draw_keep)
    shot_t = now + 2000 + rnd() % 800;
    dive_dir = 0;
    dive_t = 0;
    say = nullptr;
}

void start_round(uint32_t now) {
    go(AIM, now);
    say = nullptr;
    bot_aim = frand() * 1.6f - 0.8f;
    bot_power = 0.35f + frand() * 0.55f;
}

void start(uint32_t now, uint32_t seed, int b) {
    rng = seed ? seed : 1;
    best = b;
    round_no = you = cpu = saves = 0;
    bot_until = 0;
    last_input = now;
    start_round(now);
}

void input(Btn b, uint32_t now) {
    if (b != BTN_DOWN) return;
    last_input = now;
    if (now - phase_t < 250) return;
    switch (phase) {
    case AIM:
        aim = aim_at(now);
        go(POWER, now);
        break;
    case POWER: {
        power = power_at(now);
        over_bar = power > 0.93f;
        wide = fabsf(aim) > 1.07f;
        // keeper is not very good, he only guesses right one time in five
        kdir = zone(aim);
        if (frand() >= 0.2f) kdir = kdir == 0 ? (rnd() % 2 ? 1 : -1) : (rnd() % 2 ? 0 : -kdir);
        scored = !over_bar && !wide && kdir != zone(aim);
        if (scored) you++;
        say = over_bar ? "OVER!" : wide ? "WIDE!" : scored ? "GOAL!" : "SAVED";
        go(FLY, now);
        break;
    }
    case KEEP:
        if (!dive_dir && now < shot_t + 550) {
            dive_dir = sway_dir(now);
            // the sway just turned? a late tap still goes the old way
            if (dive_dir != sdir && sdir && now - phase_t > 300 && sway_dir(now - 300) == sdir) dive_dir = sdir;
            dive_t = now;
            sway = sway_at(now);
        }
        break;
    case OVER:
        if (now - phase_t > 600) start(now, rnd(), best);
        break;
    default:
        break;
    }
}

void goal(Canvas& c) {
    c.fill_rect(GL - 1, BAR, GL, GROUND);
    c.fill_rect(GR, BAR, GR + 1, GROUND);
    c.fill_rect(GL - 1, BAR, GR + 1, BAR + 1);
    for (int y = BAR + 6; y < GROUND; y += 6)
        for (int x = GL + 2; x < GR; x += 2) c.px(x, y);
    for (int x = GL + 6; x < GR; x += 6)
        for (int y = BAR + 3; y < GROUND; y += 2) c.px(x, y);
    c.hline(0, 127, GROUND);
}

// keeper at offset dx, leaning k (0..1) toward dir
void keeper(Canvas& c, float dx, int dir, float k) {
    float cx = 64 + dx, cy = GROUND - 12;
    if (dir == 0 || k <= 0) {
        clawd(c, (int)(cx - 18), (int)(cy - 12), pose(EYES_OPEN, ARMS_UP));
        return;
    }
    clawd_rotated(c, cx, cy - 4 * k, -dir * 70 * k, pose(dir < 0 ? EYES_LEFT : EYES_RIGHT, ARMS_UP));
}

void ball(Canvas& c, float x, float y, float r) {
    c.fill_circle(x, y, r + 1, false);
    c.ellipse(x - r, y - r, x + r, y + r);
    if (r >= 3) c.px((int)x, (int)y);
}

void banner(Canvas& c, const char* s) {
    int w = c.text_width(s, font_bold12);
    c.fill_rect(64 - w / 2 - 4, 16, 64 + w / 2 + 4, 32, false);
    c.rect(64 - w / 2 - 4, 16, 64 + w / 2 + 4, 32);
    c.text_center(64, 19, s, font_bold12);
}

void hud(Canvas& c, const char* hint) {
    char s[32];
    snprintf(s, sizeof(s), "you %d - %d cpu", you, cpu);
    c.text_center(64, 2, s, font_4x6);
    c.fill_rect(0, GROUND + 1, 127, 63, false);
    if (hint) c.text(2, 58, hint, font_4x6);
    for (int i = 0; i < ROUNDS; i++) {
        int x = 94 + i * 7;
        if (i < round_no) c.fill_circle(x, 60, 2);
        else c.ellipse(x - 2, 58, x + 2, 62);
    }
}

void draw_shoot(Canvas& c, uint32_t now) {
    goal(c);
    float tx = 64 + aim * 38, ty = 52 - power * 46;
    if (phase == AIM || phase == POWER) {
        keeper(c, 4 * sinf(now * 0.004f), 0, 0);
        float a = phase == AIM ? aim_at(now) : aim;
        float mx = 64 + a * 38;
        c.fill_rect((int)mx - 7, 15, (int)mx + 7, 29, false);
        c.ellipse(mx - 4, 18, mx + 4, 26);
        c.vline((int)mx, 16, 28);
        c.hline((int)mx - 6, (int)mx + 6, 22);
        ball(c, 64, 52, 3);
        if (phase == POWER) {
            float p = power_at(now);
            c.rect(118, 14, 124, 54);
            c.fill_rect(119, (int)(53 - p * 38), 123, 53);
            c.hline(115, 117, (int)(53 - 0.93f * 38));
        }
        hud(c, phase == AIM ? "aim" : "power");
        return;
    }
    float k = clampf((now - phase_t) / 600.0f, 0, 1);
    float dk = clampf(((float)(now - phase_t) - 120) / 300.0f, 0, 1);
    keeper(c, kdir * 26 * ease_out(dk), kdir, ease_out(dk));
    if (over_bar) ty = -6;
    float e = ease_out(k);
    float bx = 64 + (tx - 64) * e, by = 52 + (ty - 52) * e;
    if (!scored && !over_bar && !wide && k >= 1) {
        // parried away
        float u = clampf(((float)(now - phase_t) - 600) / 400.0f, 0, 1);
        bx += (kdir ? kdir : 1) * 40 * u;
        by -= 20 * u;
    }
    ball(c, bx, by, 3 - k);
    if (now - phase_t > 650 && say) banner(c, say);
    hud(c, nullptr);
}

void draw_keep(Canvas& c, uint32_t now) {
    goal(c);
    float sx = sway_at(now);
    if (dive_dir) {
        float k = ease_out(clampf((now - dive_t) / 220.0f, 0, 1));
        float dx = sway * 24 + (dive_dir * 30 - sway * 24) * k;
        keeper(c, dx, dive_dir, k);
    } else if (phase == KEEP) {
        keeper(c, sx * 24, 0, 0);
        int d = sway_dir(now);
        float ax = 64 + sx * 24 + d * 22;
        float tri[] = {ax, 22, ax - d * 4, 18, ax - d * 4, 26};
        c.fill_poly(tri, 3);
    } else {
        keeper(c, sway * 24, 0, 0);
    }
    // the warning: a blinking target where the shot will go
    if (phase == KEEP && !dive_dir && now + 1600 >= shot_t && now < shot_t + 300 && now % 300 < 200) {
        int tx = 64 + sdir * 30;
        c.fill_rect(tx - 5, 33, tx + 5, 43, false);
        c.rect(tx - 4, 34, tx + 4, 42);
        c.fill_rect(tx - 1, 37, tx + 1, 39);
    }
    if (now >= shot_t) {
        float k = clampf((now - shot_t) / 600.0f, 0, 1);
        float tx = 64 + sdir * 30, ty = 38;
        float bx = 64 + (tx - 64) * k, by = 54 + (ty - 54) * ease_out(k);
        if (phase == KEEP_DONE && !scored) {
            float u = clampf((now - shot_t - 600) / 300.0f, 0, 1);
            bx += (sdir ? sdir : 1) * 30 * u;
            by += 14 * u;
        }
        ball(c, bx, by, 6 - 4 * k);
    }
    if (phase == KEEP_DONE && now - phase_t > 100 && say) banner(c, say);
    hud(c, phase == KEEP ? (sdir == 0 && now + 1600 >= shot_t ? "stay!" : "dive!") : nullptr);
}

void draw_over(Canvas& c, uint32_t now) {
    const char* r = you > cpu ? "YOU WIN!" : you < cpu ? "CPU WINS" : "DRAW";
    c.text_center(64, 4, r, font_bold16);
    char s[32];
    snprintf(s, sizeof(s), "you %d - %d cpu", you, cpu);
    c.text_center(64, 26, s);
    int sc = you + saves;
    snprintf(s, sizeof(s), "score %d  best %d", sc, sc > best ? sc : best);
    c.text_center(64, 38, s, font_4x6);
    if (now - phase_t > 600 && (now / 500) % 2) c.text_center(64, 54, "tap to play again", font_4x6);
}

bool draw(Canvas& c, uint32_t now) {
    switch (phase) {
    case AIM:
    case POWER:
        draw_shoot(c, now);
        break;
    case FLY:
        draw_shoot(c, now);
        if (now - phase_t > 1400) {
            start_keep(now);
        }
        break;
    case KEEP:
        draw_keep(c, now);
        if (now >= shot_t + 600) {
            float kx = dive_dir ? dive_dir * 30.0f : sway_at(now) * 24;
            scored = fabsf(kx - sdir * 30) >= (dive_dir ? 14 : 20);
            if (!dive_dir) sway = sway_at(now);
            if (scored) cpu++;
            else saves++;
            say = scored ? "CPU GOAL" : "SAVED!";
            go(KEEP_DONE, now);
        }
        break;
    case KEEP_DONE:
        draw_keep(c, now);
        if (now - phase_t > 1100) {
            round_no++;
            if (round_no >= ROUNDS) go(OVER, now);
            else start_round(now);
        }
        break;
    case OVER:
        draw_over(c, now);
        if (now - phase_t > 6000 && now - last_input > 6000) return false;
        break;
    default:
        break;
    }
    return true;
}

int score() { return you + saves; }

bool bot(uint32_t now) {
    if (now < bot_until) return true;
    if (now - phase_t < 400) return false;
    bool press = false;
    if (phase == AIM) press = fabsf(aim_at(now) - bot_aim) < 0.08f;
    else if (phase == POWER) press = fabsf(power_at(now) - bot_power) < 0.05f;
    // sees the target, waits for the sway to point that way
    else if (phase == KEEP && !dive_dir && now + 1300 > shot_t)
        press = sdir == 0 ? false : (sway_dir(now) == sdir && (rnd() % 100) < 10) || now > shot_t + 350;
    if (press) bot_until = now + 80;
    return press;
}

// menu logo: the ball flying into the top corner
void icon(Canvas& c, int cx, int cy, uint32_t now) {
    int l = cx - 13, r = cx + 13, top = cy - 11, bot = cy + 11;
    for (int y = top + 4; y < bot; y += 3)
        for (int x = l + 3 + (y & 1); x < r - 1; x += 3) c.px(x, y);
    c.fill_rect(l, top, l + 1, bot);
    c.fill_rect(r - 1, top, r, bot);
    c.fill_rect(l, top, r, top + 1);
    c.hline(cx - 16, cx + 16, bot);
    // the ball shakes the net a little
    int k = (now / 150) % 6 == 0 ? 1 : 0;
    int bx = cx + 5 + k, by = top + 8 - k;
    // clear its path through the net, then two speed lines
    c.line(bx, by, bx - 9, by + 9, 5, false);
    c.line(bx - 4, by + 3, bx - 10, by + 9, 1);
    c.line(bx - 2, by + 6, bx - 6, by + 10, 1);
    c.fill_rect(bx - 4, by - 5, bx + 4, by + 5, false);
    c.fill_rect(bx - 5, by - 4, bx + 5, by + 4, false);
    c.ellipse(bx - 4, by - 4, bx + 4, by + 4);
    c.fill_rect(bx - 1, by - 1, bx + 1, by + 1);
    c.hline(bx - 1, bx + 1, by - 3);
    c.px(bx - 3, by + 1);
    c.px(bx + 3, by + 1);
}

}  // namespace

extern const Game game_penalty = {"penalty", "penalties", start, input, draw, score, bot, icon};
